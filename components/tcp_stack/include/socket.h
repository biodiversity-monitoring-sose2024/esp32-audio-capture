#ifndef SOCKET_H
#define SOCKET_H

#include <string>

/// @brief A class encapsulating the C socket with automatic cleanup
class Socket {
public:
    /// @brief Creates a new socket and tries to connect to the server
    /// is_connected should be checked before trying to perform operations on the socket
    /// @param ip The ip to connect to
    /// @param port The port to connect to
    Socket(std::string& ip, int& port);

    /// @returns A bool indicating whether the socket is connected
    [[nodiscard]] inline bool is_connected() const noexcept {
        return this->fd > 0;
    };

    /// @returns The fd of the socket
    [[nodiscard]] inline int get_fd() const noexcept {
        return this->fd;
    };

    /// @brief Properly disconnects the session and destroys the handle
    ~Socket();

private:
    const char* TAG = "socket";

    /// @brief The file descriptor for the socket
    int fd = -1;
};

#endif //SOCKET_H
