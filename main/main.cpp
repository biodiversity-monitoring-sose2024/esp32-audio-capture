#include <nvs.h>
#include <esp_err.h>
#include "nvs_flash.h"
#include "wifi.h"
#include "tcp_client.h"
#include <esp_pthread.h>
#include "storage.h"
#include <periph_sdcard.h>
#include <esp_log.h>
#include <filesystem>
#include "dirent.h"
#include "audio_recorder.h"

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
  ESP_ERROR_CHECK(storage.init());

  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir ("/sdcard")) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir (dir)) != NULL) {
      ESP_LOGI(TAG, "File: %s", ent->d_name);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    ESP_LOGE(TAG, "Could not open directory");
  }

  // Init TCP Client
  ESP_LOGI(TAG, "Init tcp client");
  Client client(CONFIG_ESP_TCP_SERVER_IP, CONFIG_ESP_TCP_SERVER_PORT);
  ESP_ERROR_CHECK(client.init());
  
  // Init Audio Recorder
  audio_recorder_config_t audio_conf = {
    .sample_rate = 48000,
    .core = 1,
    .record_path = "/sdcard",
    .buffer_size = 2048,
  };
  AudioRecorder audio(audio_conf, std::move(client));
  audio.start();
  audio.recording_thread.join();
}