#include "storage.h"
#include <esp_log.h>

esp_err_t Storage::init()
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    this->handle = periph_sdcard_init(&this->config);
    esp_err_t ret = esp_periph_start(set, this->handle);
    if (ret != ESP_OK) {
        return ret;
    }

    int retry_times = 5;
    while (retry_times --) {
        if (periph_sdcard_is_mounted(this->handle)) {
            this->is_mounted = true;
            return ESP_OK;
        }
        else vTaskDelay(500 / portTICK_PERIOD_MS);    
    }

    if (this->is_mounted) return ESP_OK;

    ESP_LOGE(this->TAG.c_str(), "Failed to mount Sd card!");

    return ESP_FAIL;
}
