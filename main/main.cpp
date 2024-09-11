#include <nvs.h>
#include <esp_err.h>
#include "nvs_flash.h"
#include "wifi.h"
#include "esp_log.h"
#include "tcp_client.h"
#include <esp_pthread.h>
#include "storage.h"
#include <periph_sdcard.h>

static const char *TAG = "main";

extern "C" void app_main()
{
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Init pthread
  auto cfg = esp_pthread_get_default_config();
  esp_pthread_set_cfg(&cfg);

  // Init wifi
  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  Wifi wifi = Wifi();
  wifi.init();

  // Init Storage
  periph_sdcard_cfg_t sdcard_cfg = {
    .card_detect_pin = 34,
    .root = "/sdcard",
    .mode = periph_sdcard_mode_t::SD_MODE_4_LINE
  };
  Storage storage(sdcard_cfg);
  storage.init();

  if (FILE* f = fopen("/sdcard/test.txt", "r")) {
    ESP_LOGI(TAG, "Exists");
    fclose(f);
  }
  else {
    ESP_LOGE(TAG, "No");
  }

  // Init TCP Client
  ESP_LOGI(TAG, "Init tcp client");
  Client client(CONFIG_ESP_TCP_SERVER_IP, CONFIG_ESP_TCP_SERVER_PORT);
  ESP_ERROR_CHECK(client.init());

  int8_t buffer[] = "Hello World!";

  int8_t size[4];
  size[0] = 0;
  size[1] = 0;
  size[2] = 0;
  size[3] = sizeof(buffer);

  while (true) {
    if (client.is_connected) {
      if (!client.enqueue(size, sizeof(size))) continue;
      client.enqueue(buffer, sizeof(buffer));
    }
  }
}