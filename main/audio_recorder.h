#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H
#include "tcp_client.h"
#include <board.h>
#include <i2s_stream.h>
#include <wav_encoder.h>
#include <wav_decoder.h>
#include <fatfs_stream.h>
#include <audio_pipeline.h>
#include "audio_element.h"

typedef struct {
    uint32_t sample_rate;
    int core;
    std::string record_path;
    int buffer_size;
} audio_recorder_config_t;

class AudioRecorder {
    public:
        AudioRecorder(audio_recorder_config_t& config, Client&& client)
            : config(config), client(client)
            {};
        void start();
        void stop();
        ~AudioRecorder();
        std::thread recording_thread;
    private:
        std::vector<std::thread*> send_tasks;

        audio_element_handle_t wav_fatfs_stream_reader;
        audio_element_handle_t wav_encoder;
        audio_element_handle_t wav_decoder;
        audio_element_handle_t wav_fatfs_stream_writer;

        audio_board_handle_t board_handle;

        // Config
        audio_recorder_config_t config;
        wav_encoder_cfg_t wav_config;
        wav_decoder_cfg_t wav_config2;
        fatfs_stream_cfg_t fatfs_config;
        i2s_stream_cfg_t i2s_config;

        // Buffers
        ringbuf_handle_t rb_i2s_to_wav;
        ringbuf_handle_t rb_wav_to_fatstream;

        audio_pipeline_handle_t pipeline;

        audio_element_handle_t fft_filter;

        Client client;
        std::string TAG = "audio_recorder";

        bool stop_requested = false;

        void run();
};

#endif