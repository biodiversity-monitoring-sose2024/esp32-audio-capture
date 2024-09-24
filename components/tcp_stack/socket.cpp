#include <lwip/sockets.h>
#include <esp_log.h>

#include "socket.h"

Socket::Socket(std::string &ip, int &port) {
    /**
     *  Create address
     */
    sockaddr_in addr{};

    // IPv4 Address
    addr.sin_family = AF_INET;

    // Set ip address
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    // Set port
    addr.sin_port = htons(port);

    // For errno returns
    int err = 0;

    /**
     * Try create socket
     */
    ESP_LOGD(this->TAG, "Trying to create socket...");

    this->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (!this->is_connected()) {
        err = errno;
        ESP_LOGE(this->TAG, "Could not create socket (%d): %s", err, strerror(err));
        return;
    }

    ESP_LOGD(this->TAG, "Socket successfully created with file descriptor: %d", this->fd);

    /**
     * Try connect to server
     */

    ESP_LOGD(this->TAG, "Trying to connect to %s:%d...", ip.c_str(), port);
    err = connect(this->fd, (const sockaddr*) &addr, sizeof(addr));
    if (err != 0) {
        err = errno;
        close(this->fd);
        this->fd = -1;
        ESP_LOGW(this->TAG, "Could not connect to %s:%d (%d): %s", ip.c_str(), port, err, strerror(err));
        return;
    }
}

Socket::~Socket() {
    // Close the socket if it is still connected
    if (this->is_connected()) close(this->fd);
}