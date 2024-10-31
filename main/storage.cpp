#include "storage.h"

#include <cstring>
#include <esp_log.h>
#include <file_utils.h>
#include <format>
#include <fstream>

FILE* log_fp = nullptr;

// This function will be called by the ESP log library every time ESP_LOG needs to be performed.
//      @important Do NOT use the ESP_LOG* macro's in this function ELSE recursive loop and stack overflow! So use printf() instead for debug messages.
int _log_vprintf(const char *fmt, va_list args) {
    static bool static_fatal_error = false;
    static const uint32_t WRITE_CACHE_CYCLE = 1;
    static uint32_t counter_write = 0;
    int iresult;

    // #1 Write to SPIFFS
    if (log_fp == nullptr) {
        printf("%s() ABORT. file handle _log_remote_fp is NULL\n", __FUNCTION__);
        return -1;
    }
    if (static_fatal_error == false) {
        iresult = vfprintf(log_fp, fmt, args);
        if (iresult < 0) {
            printf("%s() ABORT. failed vfprintf() -> disable future vfprintf(_log_remote_fp) \n", __FUNCTION__);
            // MARK FATAL
            static_fatal_error = true;
            return iresult;
        }

        // #2 Smart commit after x writes
        counter_write++;
        if (counter_write % WRITE_CACHE_CYCLE == 0) {
            /////printf("%s() fsync'ing log file on SPIFFS (WRITE_CACHE_CYCLE=%u)\n", WRITE_CACHE_CYCLE);
            fsync(fileno(log_fp));
        }
    }

    // #3 ALWAYS Write to stdout!
    return vprintf(fmt, args);
}

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
            return setup_logging();
        }

        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    if (this->is_mounted) return setup_logging();

    ESP_LOGE(this->TAG.c_str(), "Failed to mount Sd card!");

    return ESP_FAIL;
}

std::ifstream Storage::open(std::string &filename)
{
    std::ifstream in(filename, std::ifstream::binary);

    return in;
}

esp_err_t Storage::setup_logging() {
    //return ESP_OK;
    auto logfile_name = std::format("{0}/log.log", this->config.root);
    int highest_count = 1;
    for (const auto& file : get_files(this->config.root)) {
        int num = 0;
        std::sscanf(file.c_str(), "log-%d.log", &num);
        if (highest_count < num) highest_count = num;
    }

    auto rotate_name = std::format("{0}/log-{1}.log", this->config.root, highest_count);
    if (exists(logfile_name)) {
        ESP_LOGW(this->TAG.c_str(), "%s exists, moving to %s...", logfile_name.c_str(), rotate_name.c_str());
        move_file(logfile_name, rotate_name);
    }

    // Enable logging to file
    log_fp = fopen(std::format("{0}/log.log", this->config.root).c_str(), "w+");
    if (log_fp == nullptr) {
        ESP_LOGE(this->TAG.c_str(), "Failed to open log file (%d): %s", errno, strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGI(this->TAG.c_str(), "Redirecting log output to log file");
    esp_log_set_vprintf(_log_vprintf);

    return ESP_OK;
}
