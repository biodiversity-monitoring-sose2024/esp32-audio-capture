#ifndef UTIL_H
#define UTIL_H

/// @brief Converts a given array of octets into a sockaddr_in
/// @param bytes The four octets of an ip
/// @param port The port for the host
/// @returns A sockaddr_in
sockaddr_in addr_from_array(std::array<uint8_t, 4>& bytes, int& port) noexcept;

/// @brief Converts a given ip string into a sockaddr_in
/// @param ip The IPv4 IP string
/// @param port The port for the host
/// @returns A sockaddr_in
sockaddr_in addr_from_string(std::string& ip, int& port) noexcept;

/// @brief Converts a given array of octets into an std::string
/// @param bytes The four octets of an ip
/// @returns An IPv4 IP string
std::string ip_from_array(std::array<uint8_t, 4>& bytes) noexcept;

/// @brief Converts a given IPv4 IP string into a byte array of octets
/// @param ip The IPv4 IP string
/// @returns An array of four octets representing an ip
std::array<uint8_t, 4> ip_from_string(std::string& ip);

uint64_t htonll(uint64_t host64);
#endif