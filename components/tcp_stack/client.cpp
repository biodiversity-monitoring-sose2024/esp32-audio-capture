#include <esp_pthread.h>
#include <esp_log.h>
#include <esp_random.h>
#include <format>
#include <fstream>

#include "client.h"
#include "time_util.h"
#include "semaphore_lock.h"
#include "socket.h"
#include "util.h"

/**
 * Public api
 */
Client::Client(std::array<uint8_t, 6>& node_id, std::string& initial_host, int& port)
    : node_id(node_id) {

    this->server_config = server_config_t();
    this->server_config.next_data_send_timeslot = 0;
    this->server_config.servers = {
            // Initial server
            server_t {
                .host = initial_host,
                .port = port,
                .available_from = 0
            }
    };

    esp_log_level_set(this->TAG, esp_log_level_t::ESP_LOG_DEBUG);
}

void Client::init() {
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tcp_threads";
    cfg.stack_size = 8 * 1024;
    cfg.prio = 5;
    cfg.pin_to_core = 1;
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);

    ESP_LOGD(this->TAG, "Starting background threads...");

    this->update_config_thread = std::jthread(&Client::update_config, this);
    this->send_thread = std::jthread(&Client::send_queue_item, this);
}

void Client::send_file(std::string& filename, data_type_t data_type, bool delete_on_success) {
    ESP_LOGD(this->TAG, "Enqueuing %s to be sent...", filename.c_str());

    auto entry = send_queue_entry_t {
        .request = std::make_shared<data_request_t>(this->node_id, htonll(get_time()), data_type),
        .filename = filename,
        .on_success = [&]() {
            if (!delete_on_success) return;

            ESP_LOGD(this->TAG, "Deleting %s...", filename.c_str());
            remove(filename.c_str());
        }
    };

    // Push entry at the end of the queue
    {
        SemaphoreLock<1> lock(this->queue_semaphore);
        this->send_queue.push_back(entry);
    }
}

Client::~Client() {
    this->send_thread.request_stop();
    this->update_config_thread.request_stop();

    this->send_thread.join();
    this->update_config_thread.join();
}

/**
*   Threads
*/

void Client::send_queue_item(std::stop_token stop_token) {
    using namespace std::chrono_literals;

    auto default_wait_sleep_amount = 10s;
    auto default_error_sleep_amount = 1s;

    do {
        {
            SemaphoreLock<1> lock(this->config_semaphore);
            if (this->server_config.servers.empty()) {
                ESP_LOGE(this->TAG, "Something went very wrong! No servers in config! Please reset the device and ensure a working upper layer!");
                this->update_config_thread.request_stop();
                this->update_config_thread.join();
                exit(-1);
                return;
            }
        }

        send_queue_entry_t* front;
        {
            // Acquire lock
            SemaphoreLock<1> lock(this->queue_semaphore);

            // If empty wait and retry
            if (this->send_queue.empty()) {
                ESP_LOGD(this->TAG, "Send queue empty");
                // Explicitly release lock
                lock.release();
                std::this_thread::sleep_for(default_wait_sleep_amount);
                continue;
            }

            // If not empty get pointer to first element
            front = &this->send_queue.front();
        }

        ESP_LOGD(this->TAG, "In queue: %02x", front->request->type);

        // Filter out all servers that aren't available right now
        std::vector<server_t> available_servers;
        std::copy_if(this->server_config.servers.begin(), this->server_config.servers.end(), std::back_inserter(available_servers), [](server_t server) {
            return get_time() >= server.available_from;
        });

        // Get random server
        // TODO: Try all servers, if none available indicate desire to go to sleep for x time
        auto server = &available_servers[esp_random() % available_servers.size()];
        Socket socket(server->host, server->port);

        if (!socket.is_connected()) {
            std::this_thread::sleep_for(default_error_sleep_amount);
            continue;
        }

        // Send session request
        session_request_t session_request(
            this->node_id,
            100,
            100,
            front->request->type
        );

        std::string empty;

        if (!send_payload(socket.get_fd(), &session_request, empty)) {
            std::this_thread::sleep_for(default_error_sleep_amount);
            continue;
        }

        // Wait for ACK
        std::shared_ptr<payload_t> response;
        if (!receive_payload(socket.get_fd(), response)) {
            std::this_thread::sleep_for(default_error_sleep_amount);
            continue;
        }

        switch (response->type) {
            case payload_type_t::RESET:
                ESP_LOGW(this->TAG, "Response type %02x for request of type %02x", (unsigned int)response->type, (unsigned int)session_request.type);
                std::this_thread::sleep_for(default_error_sleep_amount);
                continue;
            case payload_type_t::BLOCKED: {
                ESP_LOGW(this->TAG, "Server %s:%d is currently not able to handle the request", server->host.c_str(),
                         server->port);
                // Modify config to reflect the blocked message
                auto blocked_response = std::reinterpret_pointer_cast<blocked_response_t>(response);

                SemaphoreLock<1> lock(this->queue_semaphore);
                for (auto &item: this->server_config.servers) {

                    if (server->host != item.host || server->port != item.port) continue;

                    item.available_from = get_time() + blocked_response->expected_time_of_business;
                    break;
                }
                std::this_thread::sleep_for(default_error_sleep_amount);
                continue;
            }
            case payload_type_t::ACK:
                ESP_LOGD(this->TAG, "Got ACK for message of type %02x", (unsigned int)session_request.type);
                break;
            default:
                ESP_LOGW(this->TAG, "Payload type %02x was not expected for request type %02x", (unsigned int)response->type, (unsigned int)session_request.type);
                continue;
        }

        // Send request
        if (!this->send_payload(socket.get_fd(), front->request.get(), front->filename)){
            std::this_thread::sleep_for(default_error_sleep_amount);
            continue;
        }

        // Wait for response
        if (!this->receive_payload(socket.get_fd(), response)) {
            std::this_thread::sleep_for(default_error_sleep_amount);
            continue;
        }

        switch (response->type) {
            case payload_type_t::BLOCKED:
            case payload_type_t::RESET:
                ESP_LOGW(this->TAG, "Response type %02x for request of type %02x", (unsigned int)response->type, (unsigned int)front->request->type);
                std::this_thread::sleep_for(default_error_sleep_amount);
                // If the error handler returns true then retry, else break and clean up
                if (front->on_error(response->type)) {
                    std::this_thread::sleep_for(default_error_sleep_amount);
                    continue;
                }
                else break;
            case payload_type_t::ACK:
                front->on_success();
                break;
            default: {
                bool is_handling_of_type_requested = std::find(
                        front->custom_handling_payloads.begin(),
                        front->custom_handling_payloads.end(),
                        response->type) != front->custom_handling_payloads.end();

                if (!is_handling_of_type_requested) {
                    ESP_LOGW(this->TAG, "Received response type %02x to request %02x and no custom handler was registered!", (unsigned int)response->type, (unsigned int)front->request->type);
                    break;
                }

                if (front->custom_callback(response.get())) {
                    ack_response_t ack;
                    if (!send_payload(socket.get_fd(), &ack, empty))
                        ESP_LOGE(this->TAG, "Sending %02x in response to %02x failed!", ack.type, response->type);
                }
                else {
                    reset_response_t reset;
                    if (!send_payload(socket.get_fd(), &reset, empty))
                        ESP_LOGE(this->TAG, "Sending %02x in response to %02x failed!", reset.type, response->type);
                }
            }
        }

        // Pop the request from the queue
        {
            SemaphoreLock<1> lock(this->queue_semaphore);
            this->send_queue.pop_front();
        }
    }
    while(!stop_token.stop_requested());
}

void Client::update_config(std::stop_token stop_token) {
    using namespace std::chrono_literals;

    do {
        send_queue_entry_t entry = {
            .request = std::make_shared<config_request_t>(this->node_id),
            .filename = "",
            .custom_handling_payloads = { payload_type_t::RESP_CONFIG },
            .custom_callback = [this](payload_t* response) -> bool {
                if (response->type != payload_type_t::RESP_CONFIG) {
                    ESP_LOGW(this->TAG, "Did not expect any other request type other than %02x but got %02x", payload_type_t::RESP_CONFIG, response->type);
                    return false;
                }

                auto config = reinterpret_cast<config_response_t*>(response);
                config->server_addresses_len = ntohs(config->server_addresses_len);
                // TODO: ntohl next_timeslot_in

                SemaphoreLock<1> lock(this->config_semaphore);
                if (this->server_config.next_data_send_timeslot != get_time() + config->next_timeslot_in) {
                    ESP_LOGI(this->TAG, "Timeslot changed, new one is in %llu seconds.", config->next_timeslot_in);
                }

                ESP_LOGD(this->TAG, "Got timeslot in %llu, length of addresses %d", config->next_timeslot_in, config->server_addresses_len);

                std::vector<server_t> new_servers {};
                int port = this->server_config.servers[0].port;
                for (int i = 0; i < config->server_addresses_len*4; i+=4) {
                    auto ip_string = std::format("{0}.{1}.{2}.{3}", config->server_addresses[i],config->server_addresses[i+1],config->server_addresses[i+2],config->server_addresses[i+3]);

                    auto existing_server = std::find_if(this->server_config.servers.begin(), this->server_config.servers.end(), [&](server_t& item) {
                        // We're not sending different ports
                        return item.host == ip_string;
                    });

                    if (existing_server != this->server_config.servers.end()) {
                        new_servers.push_back(*existing_server);

                        ESP_LOGD(this->TAG, "Got new server %s:%d but we knew about that one already!", ip_string.c_str(), port);
                        continue;
                    }

                    new_servers.push_back(server_t{
                        .host = ip_string,
                        .port = port,
                        .available_from = 0
                    });
                    ESP_LOGI(this->TAG, "Added new server: %s:%d", ip_string.c_str(), port);
                }

                this->server_config.servers = new_servers;

                return true;
            },
        };

        std::this_thread::sleep_for(10s);
        SemaphoreLock<1> lock(this->queue_semaphore);
        this->send_queue.push_back(entry);
    } while (!stop_token.stop_requested());
}

/**
*   Utility functions
*/
bool Client::receive_payload(const int &fd, std::shared_ptr<payload_t> &ptr) const noexcept {
    // for error handling
    int err = 0;

    // Receive the data length
    std::array<uint8_t, 4> size_buff{};
    ESP_LOGD(this->TAG, "Waiting for size of incoming message.");
    ssize_t result = recv(fd, size_buff.data(), size_buff.size(), MSG_WAITALL);

    if (result == -1) {
        // An error has been encountered
        err = errno;
        ESP_LOGE(this->TAG, "Receiving data failed: errno %d: %s", err, strerror(err));
        return false;
    }

    // Convert to host byte order and to 32 bit integer
    std::size_t received_size = ntohl(size_buff[0] | (size_buff[1] << 8) | (size_buff[2] << 16) | (size_buff[3] << 24));

    // Receive the actual message
    ESP_LOGI(this->TAG, "Waiting for payload of size %d", received_size);
    std::vector<uint8_t> input_buff = std::vector<uint8_t>(received_size);
    result = recv(fd, input_buff.data(), input_buff.size(), MSG_WAITALL);
    if (result == -1) {
        err = errno;
        ESP_LOGE("receive", "Receiving data failed: errno %d: %s", err, strerror(err));
        return false;
    }

    if (result != received_size) {
        ESP_LOGE(this->TAG, "Mismatched size encountered, expected %d bytes but got %d", received_size, result);
        return false;
    }

    // Cast result to response_t and reset the shared ptr with the result pointer;
    auto* resp = (response_t*)malloc(received_size);
    memcpy(resp, input_buff.data(), received_size);
    ptr.reset(resp);

    ESP_LOGI(this->TAG, "Got response type %02x", (unsigned int)ptr->type);
    return true;
}

/// @brief Sends a simple payload that has no special fields that don't find into heap or stack
/// @param fd The file descriptor of the socket
/// @param payload The payload to send
/// @param payload_size The payload length
/// @returns A bool indicating
bool send_simple_payload(const int& fd, payload_t* payload, size_t payload_size) {
    // Error handling
    int err = 0;

    // Convert to network byte order
    int32_t no_payload_size = htonl(payload_size);

    // Create size buffer
    char size_buffer[4] = {
            (char)(no_payload_size >> 0    & 0xff),
            (char)(no_payload_size >> 8    & 0xff),
            (char)(no_payload_size >> 16   & 0xff),
            (char)(no_payload_size >> 24   & 0xff),
            };

    // Send size buffer
    err = send(fd, size_buffer, sizeof(size_buffer), 0);
    if (err == -1) {
        err = errno;
        ESP_LOGE("simple request", "Error while sending payload length (%d): %s", err, strerror(err));
        return false;
    }

    // Send raw data
    err = send(fd, payload, payload_size, 0);
    if (err == -1) {
        err = errno;
        ESP_LOGE("simple request", "Error while sending the payload (%d): %s", err, strerror(err));
        return false;
    }

    return true;
}

/// @brief Sends a data request using a buffer for sending the file
/// @param fd The file descriptor of the socket
/// @param filename The name of the file to open
/// @returns A bool indicating whether or not the request succeeded
bool send_data_request(const int& fd, payload_t* payload, std::string& filename) {
    auto data_request = reinterpret_cast<data_request_t*>(payload);

    // Error handling
    int err = 0;

    // Try to open the file
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (file.fail()) {
        // TODO: figure out how to make this fail safe, i.e. don't lead to overfilling the sd card or queue
        ESP_LOGE("send data", "Failed to open the file: %s", filename.c_str());
        return false;
    }
    ESP_LOGD("send data", "Successfully opened file: %s", filename.c_str());
    // Don't skip whitespaces
    file.unsetf(std::ios::skipws);

    // Get file size and write it to the struct in network byte order
    uint32_t file_size = file.tellg();
    data_request->data_length = htonl(file_size);

    // Reset file position
    file.seekg(0);

    // Get payload sizes
    uint32_t header_size = sizeof(data_request_t) - sizeof(data_request->data);
    uint32_t payload_size = header_size + file_size;
    uint32_t no_payload_size = htonl(payload_size);

    // Create size buffer
    char size_buffer[4] = {
            (char)(no_payload_size >> 0    & 0xff),
            (char)(no_payload_size >> 8    & 0xff),
            (char)(no_payload_size >> 16   & 0xff),
            (char)(no_payload_size >> 24   & 0xff),
    };

    ESP_LOGD("send data", "Sending size %ld...", payload_size);
    // Send size buffer
    err = send(fd, size_buffer, sizeof(size_buffer), 0);
    if (err == -1) {
        err = errno;
        ESP_LOGE("data request", "Error while sending payload length (%d): %s", err, strerror(err));
        file.close();
        return false;
    }

    ESP_LOGD("send data", "Sending header...");
    // Send header
    err = send(fd, data_request, header_size, 0);
    if (err == -1) {
        err = errno;
        ESP_LOGE("data request", "Error while sending the header (%d): %s", err, strerror(err));
        file.close();
        return false;
    }

    ESP_LOGD("send data", "Sending data...");
    // Send data
    const int bufferSize = 8 * 1024;
    char* buffer = (char*)malloc(bufferSize);
    while (!file.eof()) {
        file.read(buffer, bufferSize);
        err = send(fd, buffer, file.gcount(), 0);
        if (err == -1) {
            err = errno;
            ESP_LOGE("data request", "Error while sending data (%d): %s", err, strerror(err));
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}

bool Client::send_payload(const int &fd, payload_t *payload, std::string &filename) const noexcept {
    ESP_LOGI(this->TAG, "Sending payload of type %02x...", (unsigned int)payload->type);

    size_t payload_size = 0;
    switch (payload->type) {
        // Requests
        case payload_type_t::REQ_CONFIG:
            payload_size = sizeof(config_request_t);
            break;
        case payload_type_t::SESSION:
            payload_size = sizeof(session_request_t);
            break;
        case payload_type_t::DATA:
            return send_data_request(fd, payload, filename);
        // Responses
        case payload_type_t::ACK:
            payload_size = sizeof(ack_response_t);
            break;
        case payload_type_t::RESET:
            payload_size = sizeof(reset_response_t);
            break;
        case payload_type_t::BLOCKED:
            payload_size = sizeof(blocked_response_t);
            break;
        default:
            ESP_LOGE(this->TAG, "Unknown payload type %02x encountered in send_payload!", payload->type);
            return false;
    }

    return send_simple_payload(fd, payload, payload_size);
}