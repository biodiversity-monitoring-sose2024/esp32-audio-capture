#ifndef WIFI_H
#define WIFI_H
#include <memory>
#include <esp_wifi_types_generic.h>
#include "esp_wifi.h"

class Wifi {
    private:
        wifi_config_t m_config;
    
    public: 
        Wifi();
        void init(void);
        esp_netif_t* interface;
};

#endif