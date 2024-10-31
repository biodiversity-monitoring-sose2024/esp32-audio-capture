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
#include "client_config.h"

class Client {
    public:
        /// @brief Creates an instance of Client with an initial server config
        /// @param config The config for this instance
        /// @param initial_host The initial host to fetch the config from and report results to
        /// @param port The port to use for all servers
        Client(client_config_t config, const std::string& initial_host, const int & port);

        /// @brief Initializes the client and starts background threads
        void start();
        
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
        const client_config_t client_config;

        /**
         * Threads
         */
        std::jthread gather_thread;
        /// @brief A background thread gathering files from the defined path and enqueuing them
        void gather(std::stop_token stop_token);

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
