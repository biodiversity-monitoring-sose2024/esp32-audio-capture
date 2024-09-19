#include "tcp_client.h"
#include <format>
#include "esp_mac.h"
#include <esp_log.h>
#include "util.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <esp_pthread.h>

/*
    UTIL
*/

bool send_request(int fd, request_t* request, std::ifstream* file = nullptr) {
    ESP_LOGI("send", "Sending request type %02x", (unsigned int)request->type);
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
                - sizeof(data_request->data); // - data 
            
            for (int i = 0; i < 8; i++)
                ESP_LOGI("send", "Struct: %04x", ((unsigned int*)data)[i]);

            uint32_t file_size = file->tellg();
            data_request->data_length = htonl(file_size);
            file->seekg(0);
            file->unsetf(std::ios::skipws);
            size += file_size;

            // Send size
            int32_t payload_size = htonl(size);
            char buff_size[4];
            buff_size[0] = payload_size >> 0    & 0xff;
            buff_size[1] = payload_size >> 8    & 0xff;
            buff_size[2] = payload_size >> 16   & 0xff;
            buff_size[3] = payload_size >> 24   & 0xff;
            send(fd, buff_size, 4, 0);
            
            // Send header
            send(fd, data, sizeof(data_request_t) - sizeof(data_request->data), 0);
            
            // Send data
            char buffer[1024];
            while (!file->eof()) {
                file->read(buffer, sizeof(buffer));
                send(fd, buffer, file->gcount(), 0);
            }
            
            return true;
        }
    
        default:
        {
            ESP_LOGE("send", "Unknown request type: %02x", (unsigned int)request->type);
            return false;
        }
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

    return true;
}

bool receive(int fd, std::shared_ptr<response_t>& ptr) {
    std::array<uint8_t, 4> size_buff;
    ESP_LOGI("receive", "Waiting for size of incoming message.");
    ssize_t recv_size = recv(fd, size_buff.data(), 4, MSG_WAITALL);
    if (recv_size != 4) {
        int err = errno;
        ESP_LOGE("receive", "Receiving data failed: errno %d: %s", err, strerror(err));
        return false;
    }
    std::size_t received_size = ntohl(size_buff[0] | (size_buff[1] << 8) | (size_buff[2] << 16) | (size_buff[3] << 24));
    
    ESP_LOGI("receive", "Waiting for payload of size %d", received_size);
    std::vector<uint8_t> input_buff = std::vector<uint8_t>(received_size);
    recv_size = recv(fd, input_buff.data(), input_buff.size(), MSG_WAITALL);
    if (recv_size != received_size) {
        int err = errno;
        ESP_LOGE("receive", "Receiving data failed: errno %d: %s", err, strerror(err));
        return false;
    }
    for (auto& c : input_buff)
        ESP_LOGW("receive", "Received data: %01x", c);
    
    response_t* resp = (response_t*)malloc(received_size);
    memcpy(resp, input_buff.data(), received_size);
    ptr.reset(resp);
    ESP_LOGI("receive", "Got response type %02x", (unsigned int)ptr->type);
    return true; 
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
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "file_transfer";
    cfg.pin_to_core = 0;
    cfg.stack_size = 8 * 1024;
    cfg.prio = 5;
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);

    this->update_config_thread = std::jthread(&Client::update_config, this);
    this->send_threads.push_back(std::jthread(&Client::send_queue_item, this));
}

void Client::send_file(std::string filename, data_type_t data_type, bool delete_on_success)
{
    auto entry = send_queue_entry_t {
        .request = std::make_shared<data_request_t>(get_mac(), get_time(), data_type, 6, nullptr),
        .filename = filename,
        .on_error = [](){ },
        .on_success = [&]() { 
            if (delete_on_success) {
                remove(filename.c_str());
            }
        }
    };
    this->queue_semaphore.acquire();
    this->send_queue.push_back(entry);
    this->queue_semaphore.release();
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

        auto session_request = session_request_t(
            get_mac(),
            100,
            100,
            elem->request->type
        );
        
        int fd = 0;
        auto addr = this->upstream_config.upstream_ips[rand() % this->upstream_config.upstream_ips.size()];
        if (!this->connect_socket(fd, addr)) {
            std::this_thread::sleep_for(1s); 
            continue;
        }

        // Send session request
        if (!send_request(fd, &session_request)) {
            std::this_thread::sleep_for(1s);
            close(fd);
            continue;
        }

        // Wait for ACK
        std::shared_ptr<response_t> response;
        if (!receive(fd, response)) continue;
        switch (response->type) {
            case response_type_t::BLOCKED:
            case response_type_t::RESET:
                ESP_LOGW(this->TAG.c_str(), "Response type %02x for request type %02x", (unsigned int)response->type, (unsigned int)session_request.type);
                close(fd);
                continue;
            default:
                close(fd);
                ESP_LOGW(this->TAG.c_str(), "Response type %02x was not expected for request type %02x", (unsigned int)response->type, (unsigned int)session_request.type);
                continue;
            case response_type_t::ACK:
                ESP_LOGW(this->TAG.c_str(), "ACK! Ready to send");
                break;
        }

        std::ifstream* file = nullptr;

        if (!elem->filename.empty()) {
            file = new std::ifstream(elem->filename, std::ios::ate | std::ios::binary); 
        }

        // Send payload
        if (!send_request(fd, elem->request.get(), file)) {
            delete file;
            close(fd);
            continue;
        }
        delete file;

        // Wait for response
        if (!receive(fd, response)) {
            close(fd);
            continue;
        }

        switch (response->type) {
            case response_type_t::BLOCKED:
            case response_type_t::RESET:
                elem->on_error();
                break;
            case response_type_t::ACK:
                elem->on_success();
                break;
            default:
                bool is_handling_of_type_requested = std::find(elem->custom_handling_payloads.begin(), elem->custom_handling_payloads.end(), response->type) != elem->custom_handling_payloads.end();
                if (!is_handling_of_type_requested) {
                    ESP_LOGW(this->TAG.c_str(), "Received response type %02x to request %02x and no custom handler was registered!", (unsigned int)response->type, (unsigned int)elem->request->type);
                    break;
                } 
                if (elem->custom_callback(response.get())) {
                    ack_response_t ack;
                    send_response(fd, &ack);
                }
                else {
                    reset_response_t reset;
                    send_response(fd, &reset);
                }
                break;
        }

        close(fd);

        // Pop request from queue when successful
        this->queue_semaphore.acquire();
        this->send_queue.pop_front();
        this->queue_semaphore.release();
    }
}

void Client::update_config(std::stop_token stop_token)
{
    using namespace std::chrono_literals;

    while (!stop_token.stop_requested()) {
        this->queue_semaphore.acquire();
        
        bool already_queued = std::find_if(this->send_queue.begin(), this->send_queue.end(), [this](send_queue_entry_t item){
            ESP_LOGI(this->TAG.c_str(), "Request type %02x == %02x: %i", item.request->type, request_type_t::REQ_CONFIG, item.request->type == request_type_t::REQ_CONFIG);
            return item.request->type == request_type_t::REQ_CONFIG;
        }) != this->send_queue.end();

        if (already_queued) {
            ESP_LOGW(this->TAG.c_str(), "Config request already queued");
            this->queue_semaphore.release();
            std::this_thread::sleep_for(1s);
            continue;
        }
    
        send_queue_entry_t entry = {
            .request = std::make_shared<config_request_t>(get_mac()),
            .custom_handling_payloads = { response_type_t::RESP_CONFIG },
            .custom_callback = [this](response_t* response){
                if (response->type != response_type_t::RESP_CONFIG)
                    return false;

                config_response_t* config_response = reinterpret_cast<config_response_t*>(response);
                this->config_semaphore.acquire();
                // TODO: convert ips
                this->upstream_config.next_data_send_timeslot = get_time() + config_response->next_timeslot_in;
                this->config_semaphore.release();

                return true;
            }
        };

        this->send_queue.push_back(entry);
        this->queue_semaphore.release();

        std::this_thread::sleep_for(60s);
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


/*
    UTIL
*/
bool Client::connect_socket(int& fd, sockaddr_in& addr) {
    // Connect
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        ESP_LOGE("send", "Could not create send socket!");
        return false;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
    int ip_port = ntohs(addr.sin_port);
    int err = connect(fd, (const sockaddr *) &addr, sizeof(addr));
    if (err != 0) {
        err = errno;
        close(fd);
        ESP_LOGE("send", "Unable to connect to %s:%d: errno %d: %s", ip_str, ip_port, err, strerror(err));
        return false;
    }

    return true;
}

bool Client::send_response(int fd, response_t* response) {
    ESP_LOGI("send", "Sending response type %02x", (unsigned int)response->type);
    int32_t size = 0;
    void* data = reinterpret_cast<void*>(response);
    switch (response->type) {
        case response_type_t::ACK:
            size = sizeof(ack_response_t);
            break;
        case response_type_t::RESET:
            size = sizeof(reset_response_t); 
            break;
    
        default:
        {
            ESP_LOGE("send", "Unknown response type: %02x", (unsigned int)response->type);
            return false;
        }
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

    return true;
}