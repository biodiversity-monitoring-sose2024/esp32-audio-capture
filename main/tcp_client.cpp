#include "tcp_client.h"
#include <esp_log.h>
#include <lwip/def.h>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <chrono>
#include <rtc_wdt.h>
#include <fstream>
#include <endian.h>

Client::Client(std::string host, int port)
{
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, host.c_str(), &dest_addr.sin_addr);
    dest_addr.sin_port = htons(port);
    this->addr = dest_addr;
}

esp_err_t Client::init(void) {
   return ESP_OK;
}

std::thread* Client::start_file_transfer(std::string filename)
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        return nullptr;
    }
    
    int err = connect(fd, (const sockaddr *) &this->addr, sizeof(this->addr));
    ESP_LOGI(this->TAG.c_str(), "After connect");
    if (err != 0) {
        err = errno;
        ESP_LOGE(this->TAG.c_str(), "Unable to create socket: errno %d: %s", err, strerror(err));
        return nullptr;
    }
    ESP_LOGI(this->TAG.c_str(), "Connected");
    ESP_LOGI(this->TAG.c_str(), "Starting thread...");
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "file_transfer";
    cfg.pin_to_core = 0;
    cfg.stack_size = 8 * 1024;
    cfg.prio = 5;
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    return new std::thread(&Client::run_file_transfer, this, filename, fd);
}

int8_t *Client::receive()
{
    return nullptr;
}

Client::~Client()
{
}

void Client::run_file_transfer(std::string filename, int fd)
{
    ESP_LOGI(this->TAG.c_str(), "FD: %d", fd);
    std::ifstream stream(filename, std::ios::binary | std::ios::ate);
    uint32_t raw_size = stream.tellg();
    if (raw_size == -1) {
        ESP_LOGE(this->TAG.c_str(), "File %s could not be opened for reading!", filename.c_str());
        close(fd);
        return;
    }
    ESP_LOGI(this->TAG.c_str(), "File %s has size %ld", filename.c_str(), raw_size);
    
    // Send file size
    int32_t file_size = htonl(raw_size); 
    char size[4];
    size[0] = file_size >> 0    & 0xff;
    size[1] = file_size >> 8    & 0xff;
    size[2] = file_size >> 16   & 0xff;
    size[3] = file_size >> 24   & 0xff;
    send(fd, &size, sizeof(size), 0);
    stream.seekg(0);
    char buffer[1024];
    while (!stream.eof()) {
        stream.read(buffer, sizeof(buffer));
        std::streamsize s = stream.gcount();

        send(fd, buffer, s, 0);
    }

    close(fd);
}