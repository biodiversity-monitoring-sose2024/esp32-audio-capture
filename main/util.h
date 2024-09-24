#ifndef UTIL_H
#define UTIL_H
#include <cstdint>
#include <esp_mac.h>
#include <esp_log.h>
#include <array>

inline std::array<uint8_t, 6> get_mac() {
    std::array<uint8_t, 6> mac{};
    esp_err_t ret = esp_read_mac(mac.data(), esp_mac_type_t::ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE("get_mac", "Could not retrieve mac address!");
    }
    return mac;
}

#endif