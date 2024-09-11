#ifndef STORAGE_H
#define STORAGE_H
#include <string>
#include <esp_err.h>
#include <periph_sdcard.h>

class Storage {
    public: 
        Storage(periph_sdcard_cfg_t& config)
            : config(config)
        {}
        esp_err_t init();
        bool is_mounted = false;
        std::ifstream open(std::string& filename);
    private:
        const std::string TAG = "storage";
        periph_sdcard_cfg_t config;
        esp_periph_handle_t handle;
};

#endif