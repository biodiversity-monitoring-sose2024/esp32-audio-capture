#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include <string>
#include <memory>
#include <sys/socket.h>
#include <thread>
#include <queue>

class Client {
    public:
        Client(std::string host, int port);
        esp_err_t init(void);
        int enqueue(int8_t* data, size_t len);
        int8_t* receive();
        bool is_connected = false;

    private:
        const std::string TAG = "tcp_client";
        std::thread run_thread;
        std::counting_semaphore<1> semaphore = std::counting_semaphore<1>(1);
        std::queue<int8_t> queue; 
        void run();
    protected:
        int fd = -1;
        struct sockaddr_in addr;
        
}; 

#endif