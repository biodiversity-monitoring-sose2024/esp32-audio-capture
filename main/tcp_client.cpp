#include "tcp_client.h"
#include <format>
#include "esp_mac.h"
#include <esp_log.h>
#include "util.h"
#include <algorithm>

/*
    Util
*/
std::vector<uint8_t> receive_bytes(int fd, size_t size) {
    std::vector<uint8_t> arr;

    ssize_t already_read = 0;
    while (already_read != size) {
        already_read += recv(fd, arr.data(), size - already_read, 0);
    }

    return arr;
} 

/*
    Public API
*/
Client::Client(std::string initial_host, int port)
{    
    // Populate config
    this->upstream_config = upstream_config_t();
    this->upstream_config.next_data_send_timeslot = 0;
    this->upstream_config.upstream_ips = {
        this->addr_from_string(initial_host, port)
    };
}

void Client::init()
{
    this->update_config_thread = std::jthread(&Client::update_config, this);
    this->send_threads.push_back(std::jthread(&Client::send_queue_item, this));
}

void Client::send_file(std::string filename, std::function<void(response_t)> callback)
{
}

Client::~Client()
{
    this->update_config_thread.join();
    for (auto& send_thread : this->send_threads) {
        send_thread.join();
    }
}

/*
    Threads
*/
void Client::send_queue_item(std::stop_token stop_token)
{
    using namespace std::chrono_literals;
    
    while (!stop_token.stop_requested()) {
        if (this->send_queue.empty()) {
            std::this_thread::sleep_for(1s);
            continue;
        }
        this->queue_semaphore.acquire();
        send_queue_entry_t* elem = &this->send_queue.front();
        this->queue_semaphore.release();
        
        std::array<uint8_t, 6> mac;
        esp_err_t ret = esp_read_mac(mac.data(), esp_mac_type_t::ESP_MAC_WIFI_STA);
        if (ret != ESP_OK) {
            ESP_LOGE(this->TAG.c_str(), "Could not retrieve mac address!");
            return;
        }

        auto session_request = session_request_t(
            mac,
            100,
            100,
            elem->request->type
        );

        send_request(&session_request, [&](bool success, response_t* response) {
            if (!success) return;

            switch (response->type) {
                case response_type_t::ACK:
                    send_request(elem->request.get(), elem->callback, elem->expected_payloads);
                    break;
                default:
                    return;
            }
        });

        // Pop request from queue when successful
        this->queue_semaphore.acquire();
        this->send_queue.pop();
        this->queue_semaphore.release();
    }
}

void Client::send_request(
        request_t* request, 
        std::function<void(bool success, response_t* response)> callback, 
        std::vector<response_type_t> expected_payloads
    ) {
    int32_t size = 0;
    void* data = reinterpret_cast<void*>(request);
    switch (request->type) {
        case request_type_t::REQ_CONFIG:
            size = sizeof(config_request_t);
            break;
        case request_type_t::SESSION:
            size = sizeof(session_request_t); 
            break;
        case request_type_t::DATA:
        {
            auto data_request = reinterpret_cast<data_request_t*>(data);
            size = sizeof(data_request_t) 
                - sizeof(data_request->data) // - pointer 
                + sizeof(uint8_t) * data_request->data_length; // + array elements
            break;
        }
    
        default:
        {
            ESP_LOGE(this->TAG.c_str(), "Unknown request type: %d", request->type);
            callback(false, nullptr);
            return;
        }
    }
        
    // Connect
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        close(fd);
        ESP_LOGE(this->TAG.c_str(), "Could not create send socket!");
        callback(false, nullptr);
        return;
    }

    auto addr = this->upstream_config.upstream_ips[rand() % this->upstream_config.upstream_ips.size()];

    int err = connect(fd, (const sockaddr *) &addr, sizeof(addr));
    if (err != 0) {
        err = errno;
        close(fd);
        // TODO specify the IP it's trying to connect to
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, str, sizeof(str));
        ESP_LOGE(this->TAG.c_str(), "Unable to connect to %s:%d: errno %d: %s", str, addr.sin_port, err, strerror(err));
        callback(false, nullptr);
        return;
    }

    // Send size + data        
    int32_t payload_size = htonl(size);
    char buff_size[4];
    buff_size[0] = payload_size >> 0    & 0xff;
    buff_size[1] = payload_size >> 8    & 0xff;
    buff_size[2] = payload_size >> 16   & 0xff;
    buff_size[3] = payload_size >> 24   & 0xff;
    send(fd, buff_size, 4, 0);
    send(fd, data, size, 0);

    // Receive size + data
    std::vector<uint8_t> buff = receive_bytes(fd, 4);

    if (buff.empty()) {
        
    }

    std::size_t received_size = ntohl(buff[0] | (buff[1] << 8) | (buff[2] << 16) | (buff[3] << 24));
    buff = receive_bytes(fd, received_size);

    // Check type of response
    response_t* response = reinterpret_cast<response_t*>(buff.data());

    bool is_expected = std::find(expected_payloads.begin(), expected_payloads.end(), response->type) != expected_payloads.end();
    if (!is_expected) {
        ESP_LOGW(this->TAG.c_str(), "Response type %d was not expected for request type %d", response->type, request->type);
    }

    callback(is_expected, response);
    close(fd);
}

void Client::update_config(std::stop_token stop_token)
{
    using namespace std::chrono_literals;
    
    std::array<uint8_t, 6> mac;
    esp_err_t ret = esp_read_mac(mac.data(), esp_mac_type_t::ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(this->TAG.c_str(), "Could not retrieve mac address!");
        return;
    }

    while (!stop_token.stop_requested()) {
        this->queue_semaphore.acquire();
        
        send_queue_entry_t entry = {
            .request = std::make_shared<config_request_t>(mac),
            .expected_payloads = { response_type_t::RESP_CONFIG, response_type_t::BLOCKED, response_type_t::RESET },
            .callback = [this](bool success, response_t* response){
                if (!success) return;

                switch (response->type) {
                    case response_type_t::BLOCKED:
                    case response_type_t::RESET:
                        ESP_LOGW(this->TAG.c_str(), "Currently unable to fetch config from server, retrying...");
                        break;
                    case response_type_t::RESP_CONFIG:
                    {
                        this->config_semaphore.acquire();
                        config_response_t* config_response = reinterpret_cast<config_response_t*>(response);
                        // TODO: convert ips
                        this->upstream_config.next_data_send_timeslot = get_time() + config_response->next_timeslot_in;
                        this->config_semaphore.release();
                        break;
                    }
                    case response_type_t::ACK:
                        break;
                }
            }
        };

        this->send_queue.push(entry);
        this->queue_semaphore.release();

        std::this_thread::sleep_for(1s);
    }
}

/*
    Util
*/
sockaddr_in Client::addr_from_array(std::array<uint8_t, 4> ip_bytes, int port)
{
    std::string ip = ip_from_array(ip_bytes);
    return addr_from_string(ip, port);
}

sockaddr_in Client::addr_from_string(std::string ip, int port)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    addr.sin_port = htons(port);
    return addr;
}

std::array<uint8_t, 4> Client::ip_from_string(std::string ip)
{
    std::string delimiter = ".";
    std::array<uint8_t, 4> arr;

    for (int i = 0; i < arr.max_size(); i++) {
        size_t pos = ip.find(delimiter);
        if (pos == std::string::npos) {
            throw std::invalid_argument(std::format("Unexpected IP length: {0}", ip));
        }

        arr[i] = std::stoi(ip.substr(0, pos));
        ip.erase(0, pos + delimiter.length()); 
    }

    return arr;
}

std::string Client::ip_from_array(std::array<uint8_t, 4> ip_bytes)
{
    return std::format("{0}.{1}.{2}.{3}", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
}
