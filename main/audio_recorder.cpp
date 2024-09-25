#include "audio_recorder.h"
#include "util.h"
#include <esp_log.h>
#include <es8388.h>

#include "audio_recorder.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "util.h"
#include <esp_log.h>
#include <es8388.h>
#include "dsps_fft_recognition.h"
#include "fatfs_stream.h"

void AudioRecorder::start()
{
    this->board_handle = audio_board_init();
    audio_hal_ctrl_codec(this->board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(this->board_handle->audio_hal, 100);

    // Hier wird der I2S-Stream entfernt und durch einen WAV-Lesestream ersetzt.
    
    // WAV-Datei von der SD-Karte lesen (statt Mikrofon als Quelle)
    this->fatfs_config = FATFS_STREAM_CFG_DEFAULT();
    this->fatfs_config.type = AUDIO_STREAM_READER; // Eingangsquelle jetzt eine WAV-Datei
    this->fatfs_config.task_core = this->config.core; // Task auf dem angegebenen Kern ausführen
    
    // FATFS-Stream für WAV-Dateien (Input)
    this->wav_fatfs_stream_reader = fatfs_stream_init(&this->fatfs_config);


    // wav_decoder
     this->wav_config2 = DEFAULT_WAV_DECODER_CONFIG();
     this->wav_decoder = wav_decoder_init(&this->wav_config2);



    
    // WAV-Encoder-Konfiguration für die Ausgabe auf SD
    this->wav_config = DEFAULT_WAV_ENCODER_CONFIG();
    this->wav_encoder = wav_encoder_init(&this->wav_config);

    // FATFS-Stream für WAV-Dateien (Output)
    this->fatfs_config = FATFS_STREAM_CFG_DEFAULT();
    this->fatfs_config.type = AUDIO_STREAM_WRITER;
    this->fatfs_config.task_core = 1;
    this->wav_fatfs_stream_writer = fatfs_stream_init(&this->fatfs_config);

    // Audioelement-Infos konfigurieren
    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(this->wav_fatfs_stream_reader, &info);
    audio_element_getinfo(this->wav_fatfs_stream_writer, &info);
    info.bits = 16;  // Setze die Bits der Audioinformationen
    info.sample_rates = this->config.sample_rate;  // Setze die Sample-Rate
    info.channels = 1;  // Setze die Kanalanzahl (1 für Mono)
    audio_element_setinfo(this->wav_fatfs_stream_reader, &info);
    audio_element_setinfo(this->wav_fatfs_stream_writer, &info);

    // Erstelle den Ringbuffer
    this->rb_wav_to_fatstream = rb_create(this->config.buffer_size, 1);
    audio_element_set_input_ringbuf(this->wav_fatfs_stream_writer, this->rb_wav_to_fatstream);
    audio_element_set_output_ringbuf(this->wav_encoder, this->rb_wav_to_fatstream);

    // Audio-Pipeline initialisieren
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    this->pipeline = audio_pipeline_init(&pipeline_cfg);

    // FFT-Analyser in die Pipeline einfügen
    audio_element_handle_t fft = FFTAnalyser_init();
    this->fft_filter = fft;


    // Event Interface für die Pipeline
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    esp_periph_config_t set_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    auto set = esp_periph_set_init(&set_cfg);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);


    this->recording_thread = std::thread(&AudioRecorder::run, this);
    audio_event_iface_msg_t msg;
    while (1) {
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(this->TAG.c_str(), "[ E ] Event interface error: %d", ret);
        }
    }
}


void AudioRecorder::stop()
{
}

AudioRecorder::~AudioRecorder()
{

}

void AudioRecorder::run()
{
    using namespace std::chrono_literals;
    while(!stop_requested) { 
        ESP_LOGI(this->TAG.c_str(), "Starting run:"); 
        audio_pipeline_register(this->pipeline, this->wav_fatfs_stream_reader, "reader");
        audio_pipeline_register(this->pipeline, this->wav_decoder, "decoder");
        audio_pipeline_register(this->pipeline, this->fft_filter, "fft_filter");
        audio_pipeline_register(this->pipeline, this->wav_encoder, "encoder");
        audio_pipeline_register(this->pipeline, this->wav_fatfs_stream_writer, "writer");
        const char* link_tag[5] = {"reader", "decoder", "fft_filter", "encoder", "writer"}; 
        audio_pipeline_link(this->pipeline, &link_tag[0], 3);

        const char *wav_file_path = "/sdcard/vogel_aufnahme_zweiten_fuenf.wav";  // Setze den Pfad zu deiner WAV-Datei
        audio_element_set_uri(this->wav_fatfs_stream_reader, wav_file_path);    
    
        std::string filename = this->config.record_path + "/" + std::to_string(get_time()) + ".wav";
        audio_element_set_uri(this->wav_fatfs_stream_writer, filename.c_str());        
        audio_pipeline_run(this->pipeline);

        ESP_LOGI(this->TAG.c_str(), "[ * ] Recording %s...", filename.c_str()); 
        std::this_thread::sleep_for(39s);
        
        audio_pipeline_stop(this->pipeline);
        audio_pipeline_wait_for_stop(this->pipeline);
        audio_pipeline_terminate(this->pipeline);

        if (birdDetected) {
            ESP_LOGI(this->TAG.c_str(), "[ * ] Sending %s...", filename.c_str());  
            auto thread = this->client.start_file_transfer(filename);
            thread->detach();
            delete thread;
        }else {
            ESP_LOGI(this->TAG.c_str(), "No bird detected, skipping file transfer!");  
        }
    }
}