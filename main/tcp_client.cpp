#include "tcp_client.h"
#include <esp_log.h>
#include <lwip/def.h>
#include <esp_pthread.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <chrono>
#include <rtc_wdt.h>

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
        return esp_err_t(-1);
    }

    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &this->addr.sin_addr, str, sizeof(str));

    ESP_LOGI(this->TAG.c_str(), "Configured to connect to %s:%d on fd %i", str, this->addr.sin_port, this->fd);

    int err = connect(this->fd, (const sockaddr *) &this->addr, sizeof(this->addr));
    if (err != 0) {
        err = errno;
        ESP_LOGE(this->TAG.c_str(), "Unable to create socket: errno %d: %s", err, strerror(err));
        return esp_err_t(-1);
    }
    
    ESP_LOGI(this->TAG.c_str(), "Connected");

    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tcp_client";
    cfg.pin_to_core = 1;
    cfg.stack_size = 3 * 1024;
    cfg.prio = 5;
    cfg.inherit_cfg = true;

    esp_pthread_set_cfg(&cfg);
    this->run_thread = std::thread(&Client::run, this);

    return esp_err_t(0);
}



int Client::enqueue(int8_t* data, size_t len)
{
    this->semaphore.acquire();
    if (this->queue.size() > 4) {
        this->semaphore.release();
        return 0;
    }

    for (size_t i = 0; i < len; i++)
        this->queue.emplace(data[i]);
    this->semaphore.release();

    return 1;
}

int8_t *Client::receive()
{
    return nullptr;
}

void Client::run()
{
    rtc_wdt_protect_off();
    rtc_wdt_enable();         // Turn it on manually
    rtc_wdt_set_time(RTC_WDT_STAGE0, 20000); 

    using namespace std::chrono_literals;
    this->is_connected = true;
    while (true) {
        rtc_wdt_feed();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        this->semaphore.acquire();
        int8_t buffer[1024];
        int read = 0;
        for (int i = 0; i < sizeof(buffer) && !this->queue.empty(); i++) {
            buffer[i] = this->queue.front();
            this->queue.pop();
            read++;
        }
        this->semaphore.release();

        if (read == 0)  {
            continue;
        }
        ESP_LOGI(this->TAG.c_str(), "Sending %d bytes", read);

        int err = send(this->fd, buffer, read, 0);
        if (err < 0) {
            ESP_LOGE(this->TAG.c_str(), "Error during sending: errno %d", errno);
        }
    }
}
