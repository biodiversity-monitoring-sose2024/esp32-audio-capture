#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include <string>
#include <memory>
#include <sys/socket.h>
#include <thread>
#include <queue>
#include "storage.h"
#include "messages.h"
#include <functional>
#include <stop_token>

typedef struct {
    uint64_t next_data_send_timeslot;
    std::vector<sockaddr_in> upstream_ips;
} upstream_config_t;

typedef struct send_queue_entry_t {
    std::shared_ptr<request_t> request;
    std::string filename = "";
    std::function<void(void)> on_error = []{};
    std::function<void(void)> on_success = []{};
    std::vector<response_type_t> custom_handling_payloads = { };
    std::function<bool(response_t*)> custom_callback = [](response_t*){ return true; };
} send_queue_entry_t;

class Client {
    public:
        /// @brief Creates an instance of Client with an initial server config
        /// @param initial_host The initial host to fetch the config from and report results to
        /// @param port The port to use for all servers
        Client(std::string initial_host, int port);

        /// @brief Initializes the client and starts background threads
        void init();

        /// @brief Queues to send a file to a server
        /// @param filename The full path of the file to send
        /// @param callback A callback on success
        void send_file(std::string filename, data_type_t data_type, bool delete_on_success = true);
        
        /// @brief Destroys the client and frees all resources
        ~Client();
    private:
        const std::string TAG = "tcp_client";
        std::counting_semaphore<1> queue_semaphore {1};
        std::counting_semaphore<1> config_semaphore {1};
        /// @brief A queue containing elements to be sent
        std::deque<send_queue_entry_t> send_queue;
        /// @brief The config containing configuration information
        /// about upstream servers
        upstream_config_t upstream_config;

        /**
         * Threads
         */
        std::vector<std::jthread> send_threads;
        /// @brief A background thread periodically updating the config
        void send_queue_item(std::stop_token stop_token);

        std::jthread update_config_thread;
        /// @brief A background threads fetching items from the queue to send
        void update_config(std::stop_token stop_token);
        sockaddr_in addr_from_array(std::array<uint8_t,4> ip_bytes, int port);
        sockaddr_in addr_from_string(std::string ip, int port);
        std::array<uint8_t, 4> ip_from_string(std::string ip);
        std::string ip_from_array(std::array<uint8_t, 4> ip_bytes);

        // UTIL
        
        /// @brief Tries to create a connection to the given address
        /// @param fd The int to write the file descriptor to if successful
        /// @param addr The address to connect to
        /// @return A bool indicating whether or not the attempt was successful
        bool connect_socket(int& fd, sockaddr_in& addr);

        /// @brief Sends a response using a given socket
        /// @param fd The file descriptor for the socket 
        /// @param response The response to send
        /// @return A bool indicating whether or not the attempt was successful
        bool send_response(int fd, response_t* response);
}; 




#endif