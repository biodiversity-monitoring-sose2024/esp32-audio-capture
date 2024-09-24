#ifndef SERVER_CONFIG
#define SERVER_CONFIG

#include <cstdint>
#include <map>

/// @brief Defines a server the device can connect to
typedef struct {
    /// @brief The host
    std::string host;

    /// @brief The port
    int port;

    /// @brief A timestamp in unix seconds from when this server can be used
    uint64_t available_from;
} server_t;

/// @brief The config defining the next data send window and upstream server 
typedef struct {
    /// @brief The next time the device should send data
    uint64_t next_data_send_timeslot;

    /// @brief The upstream servers that are available
    /// the value indicates from when the server can be used
    std::vector<server_t> servers;
} server_config_t;

#endif