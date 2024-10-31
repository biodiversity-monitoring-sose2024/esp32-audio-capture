#ifndef DEVICE_UTILS_H
#define DEVICE_UTILS_H
#include <array>
#include <cstdint>

/// @brief Gets the mac address for the device as byte array
std::array<uint8_t, 6> get_mac() noexcept;

#endif //DEVICE_UTILS_H
