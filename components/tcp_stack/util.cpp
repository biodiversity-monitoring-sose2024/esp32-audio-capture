#include <string>
#include <lwip/sockets.h>
#include <array>
#include <stdexcept>
#include <format>

#include "util.h"

/**
* Address helpers
*/
sockaddr_in addr_from_array(std::array<uint8_t, 4>& bytes, int& port) noexcept {
    std::string ip = ip_from_array(bytes);
    return addr_from_string(ip, port);
}

sockaddr_in addr_from_string(std::string& ip, int& port) noexcept {
    sockaddr_in addr{};

    // IPv4 Address
    addr.sin_family = AF_INET;

    // Set ip address
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    // Set port
    addr.sin_port = htons(port);

    return addr;
}

std::string ip_from_array(std::array<uint8_t, 4>& bytes) noexcept {
    return std::format("{0}.{1}.{3}.{3}", bytes[0], bytes[1], bytes[2], bytes[3]);
}

std::array<uint8_t, 4> ip_from_string(std::string& ip) {
    // . as delimiter between octets
    std::string delimiter = ".";

    // we are expecting 4 octets
    std::array<uint8_t, 4> arr{};

    // Loop over the string 4 times and parse the octets as int
    for (int i = 0; i < arr.max_size(); i++) {
        size_t pos = ip.find(delimiter);
        if (pos == std::string::npos) {
            // If we have reached the end before we have looped 4 times we throw an exception since it was not a valid ip
            throw std::invalid_argument(std::format("Unexpected IP length: {0}", ip));
        }

        arr[i] = std::stoi(ip.substr(0, pos));
        ip.erase(0, pos + delimiter.length());
    }

    return arr;
}

uint64_t htonll(uint64_t host64) {
    // Detect system endianness (this part)
    uint32_t test_value = 1;
    if (*(uint8_t *)&test_value == 1) {
        // Little-endian system: need to swap bytes
        return ((uint64_t)htonl((uint32_t)(host64 & 0xFFFFFFFF)) << 32) | htonl((uint32_t)(host64 >> 32));
    } else {
        // Big-endian system: no need to swap bytes
        return host64;
    }
}