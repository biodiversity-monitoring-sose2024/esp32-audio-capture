#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <lwip/sockets.h>
#include <functional>
#include <stop_token>
#include <thread>
#include <queue>
#include <memory>

#include "data_type.h"
#include "server_config.h"
#include "payloads.h"
#include "queue_entry.h"

class Client {
    public:
        /// @brief Creates an instance of Client with an initial server config
        /// @param node_id The mac of the device as a byte array
        /// @param initial_host The initial host to fetch the config from and report results to
        /// @param port The port to use for all servers
        Client(std::array<uint8_t, 6>& node_id, std::string& initial_host, int& port);

        /// @brief Initializes the client and starts background threads
        void init();

        /// @brief Queues to send a file to a server
        /// @param filename The full path of the file to send
        /// @param callback A callback on success
        void send_file(std::string& filename, data_type_t data_type, bool delete_on_success = true);
        
        /// @brief Destroys the client and frees all resources
        ~Client();
    private:
        const char* TAG = "tcp_client";

        std::counting_semaphore<1> queue_semaphore {1};
        /// @brief A queue containing elements to be sent
        std::deque<send_queue_entry_t> send_queue;

        /**
         * Config
         */
        /// @brief The config containing configuration information
        /// about upstream servers
        server_config_t server_config;
        std::counting_semaphore<1> config_semaphore {1};
        std::array<uint8_t, 6> node_id;

        /**
         * Threads
         */
        std::jthread send_thread;
        /// @brief A background thread periodically updating the config
        void send_queue_item(std::stop_token stop_token);

        std::jthread update_config_thread;
        /// @brief A background threads fetching items from the queue to send
        void update_config(std::stop_token stop_token);
        /**
         * Utility functions
         */

        /// @brief Sends a request using a given socket
        /// @param fd The file descriptor of the socket
        /// @param payload The payload to send
        /// @param file If the request is a data request, the file to send
        /// @return A bool indicating whether or not the attempt was successful
        bool send_payload(const int& fd, payload_t* payload, std::string& filename) const noexcept;

        /// @brief Waits for a payload
        /// @param fd The file descriptor of the socket
        /// @param ptr The shared pointer to write the result to
        /// @returns A bool indicating whether or not the attempt was successful
        bool receive_payload(const int& fd, std::shared_ptr<payload_t>& ptr) const noexcept;
};

#endif
