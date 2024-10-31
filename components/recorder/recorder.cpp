#include "recorder.h"
#include "util.h"

#include <esp_log.h>
#include <esp_pthread.h>
#include <file_utils.h>
#include <format>
#include <utility>

Recorder::Recorder(recorder_config_t config) noexcept
    : config(std::move(config)) {
    esp_log_level_set(this->TAG, esp_log_level_t::ESP_LOG_DEBUG);

    ESP_LOGD(this->TAG, "Configuring the audio board");
    configure_audio_board(this->config.audio_board);

    ESP_LOGD(this->TAG, "Creating pipeline");
    /*
     *  Configs
     */
    this->i2s_stream_cfg = get_i2s_stream_cfg(this->config.core, this->config.sample_rate);
    this->wav_encoder_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    this->fatfs_stream_cfg = FATFS_STREAM_CFG_DEFAULT();
    this->fatfs_stream_cfg.type = AUDIO_STREAM_WRITER;
    this->fatfs_stream_cfg.task_core = this->config.core;

    /*
     *  Create Audio elements
     */
    this->i2s_stream_reader = i2s_stream_init(&this->i2s_stream_cfg);
    i2s_stream_set_clk(this->i2s_stream_reader, this->config.sample_rate, this->config.bits, this->config.channels);
    this->wav_encoder = wav_encoder_init(&this->wav_encoder_cfg);
    this->fatfs_stream_writer = fatfs_stream_init(&this->fatfs_stream_cfg);

    /*
     * Set all audio elements to the defined sample rate, bit depth and channel amount
     */
    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(this->i2s_stream_reader, &info);
    audio_element_getinfo(this->wav_encoder, &info);
    audio_element_getinfo(this->fatfs_stream_writer, &info);

    info.bits = this->config.bits;
    info.channels = this->config.channels;
    info.sample_rates = this->config.sample_rate;

    audio_element_setinfo(this->i2s_stream_reader, &info);
    audio_element_setinfo(this->wav_encoder, &info);
    audio_element_setinfo(this->fatfs_stream_writer, &info);

    /*
     * Create and configure ringbuffers
     */
    this->rb_i2s_to_wav = rb_create(I2S_STREAM_RINGBUFFER_SIZE, 1);
    this->rb_wav_to_fatfs = rb_create(WAV_ENCODER_RINGBUFFER_SIZE, 1);

    audio_element_set_input_ringbuf(this->wav_encoder, this->rb_i2s_to_wav);
    audio_element_set_input_ringbuf(this->fatfs_stream_writer, this->rb_wav_to_fatfs);

    audio_element_set_output_ringbuf(this->i2s_stream_reader, this->rb_i2s_to_wav);
    audio_element_set_output_ringbuf(this->wav_encoder, this->rb_wav_to_fatfs);

    /*
     * Create pipeline
     */
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    this->pipeline = audio_pipeline_init(&pipeline_cfg);

    /*
     * Ensure paths exist
     */
    ESP_LOGD(this->TAG, "Ensuring paths exist");
    ensure_base_path_exists(this->config.record_path);
    ensure_base_path_exists(this->config.store_path);
}


void Recorder::start() noexcept {
    auto cfg = esp_pthread_get_default_config();
    cfg.thread_name = "record_thread";
    cfg.stack_size = 8 * 1024;
    cfg.prio = 5;
    cfg.pin_to_core = this->config.core;
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);

    ESP_LOGD(this->TAG, "Starting record thread");
    this->record_thread = std::jthread(&Recorder::run, this);
}

Recorder::~Recorder() {
    this->record_thread.request_stop();
    this->record_thread.join();
}

void Recorder::run(std::stop_token stop_token) const noexcept {
    do {
        /*
         *  Re-register pipeline elements
         */
        audio_pipeline_register(this->pipeline, this->i2s_stream_reader, "reader");
        audio_pipeline_register(this->pipeline, this->wav_encoder, "encoder");
        audio_pipeline_register(this->pipeline, this->fatfs_stream_writer, "writer");

        const char* link_tag[3] = { "reader", "encoder", "writer" };
        audio_pipeline_link(this->pipeline, link_tag, 3);

        /*
         *  Get new filename and set the pipeline output to it
         */
        std::string filename = get_filename();
        std::string record_path = std::format("{0}/{1}", this->config.record_path, filename);
        std::string store_path = std::format("{0}/{1}", this->config.store_path, filename);

        audio_element_set_uri(this->fatfs_stream_writer, record_path.c_str());

        audio_pipeline_run(this->pipeline);

        ESP_LOGI(this->TAG, "Recording to %s for %lld seconds", record_path.c_str(), this->config.record_length.count());
        std::this_thread::sleep_for(this->config.record_length);

        /*
         *  Stop the pipeline
         */
        audio_pipeline_stop(this->pipeline);
        audio_pipeline_wait_for_stop(this->pipeline);
        audio_pipeline_terminate(this->pipeline);

        /*
         *  Move finished file to the store folder
         */
        ESP_LOGI(this->TAG, "Moving %s to %s", record_path.c_str(), store_path.c_str());
        move_file(record_path, store_path);
    }
    while (!stop_token.stop_requested());
}

