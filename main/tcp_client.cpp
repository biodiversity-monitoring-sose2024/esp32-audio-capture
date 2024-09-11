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
    this->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (this->fd < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

void Client::start_file_transfer(Storage storage, std::string filename)
{
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "file_transfer";
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    this->file_thread = std::thread(&Client::run_file_transfer, this, storage, filename);
}

int8_t *Client::receive()
{
    return nullptr;
}

void Client::run_file_transfer(Storage storage, std::string filename)
{
    int err = connect(this->fd, (const sockaddr *) &this->addr, sizeof(this->addr));
    if (err != 0) {
        err = errno;
        ESP_LOGE(this->TAG.c_str(), "Unable to create socket: errno %d: %s", err, strerror(err));
        //*result = ESP_FAIL;
        return;
    }
    ESP_LOGI(this->TAG.c_str(), "Connected");

    std::ifstream stream = storage.open(filename);
    // Send file size
    int32_t file_size = htonl(stream.tellg()); 
    char size[4];
    size[0] = file_size >> 0    & 0xff;
    size[1] = file_size >> 8    & 0xff;
    size[2] = file_size >> 16   & 0xff;
    size[3] = file_size >> 24   & 0xff;
    send(fd, &size, sizeof(size), 0);

    std::vector<char> buffer(1024, 0);
    while (!stream.eof()) {
        stream.read(buffer.data(), buffer.size());
        std::streamsize s = stream.gcount();

        send(this->fd, buffer.data(), s, 0);
    }
    //*result = ESP_OK;
    return;
}