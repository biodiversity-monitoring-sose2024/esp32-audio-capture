#ifndef RECORDER_H
#define RECORDER_H

#include <thread>
#include <board.h>
#include <i2s_stream.h>
#include <wav_encoder.h>
#include <fatfs_stream.h>
#include <audio_pipeline.h>

#include "recorder_config.h"

/// @brief A class responsible for recording files and making them ready to send
class Recorder {
public:
    /// @brief Creates a new instance of the audio recorder
    /// @param config The config to use
    explicit Recorder(recorder_config_t config) noexcept;

    /// @brief Creates the necessary components and starts the record thread
    void start() noexcept;

    /// @brief Stops the record thread and releases all resources
    ~Recorder();
private:
    const char* TAG = "recorder";

    /// @brief The config for this instance
    const recorder_config_t config;

    /// @brief The thread responsible for orchestrating the recording process
    std::jthread record_thread;

    /// @brief The method is invoked by the record thread
    void run(std::stop_token stop_token) const noexcept;

    /**
     *  Elements required for the pipeline
     */
    /// @brief The main pipeline responsible for recording, encoding and writing the audio
    audio_pipeline_handle_t pipeline{};

    /*
     *  Audio elements
     *
     *  responsible for reading from the i2s stream, encoding and saving the audio data
     */
    /// @brief Reads from the i2s ports
    audio_element_handle_t i2s_stream_reader;

    /// @brief Encodes a stream to WAV
    audio_element_handle_t wav_encoder;

    /// @brief Writes a stream to fs
    audio_element_handle_t fatfs_stream_writer;

    /*
     *  Audio element configs
     */
    /// @brief The config for the i2s stream reader
    i2s_stream_cfg_t i2s_stream_cfg{};

    /// @brief The config for the wav encoder
    wav_encoder_cfg_t wav_encoder_cfg{};

    /// @brief The config for the fatfs stream writer
    fatfs_stream_cfg_t fatfs_stream_cfg{};

    /*
     *  Ring buffers
     *
     *  used to exchange data between audio elements in a pipeline
     */
    /// @brief The ringbuffer between the i2s stream reader and the wav encoder
    ringbuf_handle_t rb_i2s_to_wav;

    /// @brief The ringbuffer between the wav encoder and the fatfs stream writer
    ringbuf_handle_t rb_wav_to_fatfs;
};

#endif