#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H
#include "client.h"
#include <board.h>
#include <i2s_stream.h>
#include <wav_encoder.h>
#include <fatfs_stream.h>
#include <audio_pipeline.h>
#include <memory>

typedef struct {
    uint32_t sample_rate;
    int core;
    std::string record_path;
    int buffer_size;
} audio_recorder_config_t;

class AudioRecorder {
    public:
        AudioRecorder(audio_recorder_config_t& config, std::shared_ptr<Client>& client)
            : config(config), client(client)
            {};
        void start();
        void stop();
        ~AudioRecorder();
        std::thread recording_thread;
    private:
        std::vector<std::thread*> send_tasks;

        audio_element_handle_t i2s_stream_reader;
        audio_element_handle_t wav_encoder;
        audio_element_handle_t wav_fatfs_stream_writer;

        audio_board_handle_t board_handle;

        // Config
        audio_recorder_config_t config;
        wav_encoder_cfg_t wav_config;
        fatfs_stream_cfg_t fatfs_config;
        i2s_stream_cfg_t i2s_config;

        // Buffers
        ringbuf_handle_t rb_i2s_to_wav;
        ringbuf_handle_t rb_wav_to_fatstream;

        audio_pipeline_handle_t pipeline;

        std::shared_ptr<Client> client;
        std::string TAG = "audio_recorder";

        bool stop_requested = false;

        void run();
};

#endif