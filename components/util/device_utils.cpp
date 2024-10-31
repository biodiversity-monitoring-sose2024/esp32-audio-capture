#include "device_utils.h"

#include <esp_err.h>
#include <esp_log.h>

#include "esp_mac.h"

std::array<uint8_t, 6> get_mac() noexcept {
    std::array<uint8_t, 6> mac{};
    esp_err_t ret = esp_read_mac(mac.data(), esp_mac_type_t::ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE("get_mac", "Could not retrieve mac address!");
    }
    return mac;
}
