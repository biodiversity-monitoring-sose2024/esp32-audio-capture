#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include <string>
#include <memory>
#include <sys/socket.h>
#include <thread>
#include <queue>
#include "storage.h"

class Client {
    public:
        Client(std::string host, int port);
        esp_err_t init(void);
        void start_file_transfer(std::string filename);
        int8_t* receive();

    private:
        const std::string TAG = "tcp_client";
        // File transfer
        std::thread file_thread;
        void run_file_transfer();
    protected:
        int fd = -1;
        struct sockaddr_in addr;
        
}; 

#endif