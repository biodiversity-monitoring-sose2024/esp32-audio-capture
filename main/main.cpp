#include <device_utils.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_pthread.h>
#include <filesystem>
#include <format>
#include <nvs.h>
#include <periph_sdcard.h>
#include <recorder.h>
#include <recorder_config.h>
#include "client.h"
#include "nvs_flash.h"
#include "storage.h"
#include "wifi.h"

static const char *TAG = "main";

extern "C" void app_main()
{
    using namespace std::chrono_literals;
    esp_log_level_set("sdmmc_req", esp_log_level_t::ESP_LOG_INFO);
    esp_log_level_set("sdmmc_cmd", esp_log_level_t::ESP_LOG_INFO);
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

    // Init Storage
    periph_sdcard_cfg_t sdcard_cfg = {
        .card_detect_pin = 34,
        .root = "/sdcard",
        .mode = periph_sdcard_mode_t::SD_MODE_4_LINE
    };
    Storage storage(sdcard_cfg);
    ESP_ERROR_CHECK(storage.init());

    // Init wifi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    auto wifi = Wifi();
    wifi.init();

    // Init Recorder
    recorder_config_t recorder_config = {
        .record_path = std::format("{0}/{1}",sdcard_cfg.root, CONFIG_RECORDING_RECORD_DIR),
        .store_path = std::format("{0}/{1}", sdcard_cfg.root, CONFIG_RECORDING_STORE_DIR),
        .record_length = std::chrono::seconds(CONFIG_RECORDING_RECORD_SECONDS),
        .sample_rate = CONFIG_RECORDING_AUDIO_SAMPLE_RATE,
        .bits = CONFIG_RECORDING_AUDIO_BIT_DEPTH,
        .channels = CONFIG_RECORDING_AUDIO_CHANNELS,
        .core = 1,
        .audio_board = audio_board_init()
    };
    Recorder recorder(recorder_config);
#ifdef CONFIG_RECORDING_ENABLE
    recorder.start();
#else
    ESP_LOGI(TAG, "Recording disabled!");
#endif

    // Init TCP Client
    std::string host = CONFIG_TCP_STACK_SERVER_IP;
    int port = CONFIG_TCP_STACK_SERVER_PORT;
    client_config_t client_config = {
        .mac = get_mac(),
        .file_dir = recorder_config.store_path,
        .queue_dir = std::format("{0}/{1}", sdcard_cfg.root, CONFIG_TCP_STACK_FINAL_DIR),
        .delete_after_send = CONFIG_TCP_STACK_DELETE
    };
    Client client(client_config, host, port);
#ifdef CONFIG_TCP_STACK_ENABLE
        client.start();
#else
    ESP_LOGI(TAG, "TCP Stack disabled!");
#endif

    while (true) {
        std::this_thread::sleep_for(100s);
    }
}