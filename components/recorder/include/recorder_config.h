#ifndef RECORDER_CONFIG_H
#define RECORDER_CONFIG_H

typedef struct {
    /// @brief The path to which the file should be recorded
    std::string record_path;

    /// @brief The path to which to move files after recording
    std::string store_path;

    /// @brief The length of the recordings
    std::chrono::seconds record_length;

    /// @brief The sample rate at which to record at
    int sample_rate = 48000;

    /// @brief The bit depth of the recording
    int bits = 16;

    /// @brief The channels of the stream
    int channels = 1;

    /// @brief The core on which the record thread should run
    int core = 1;

    /// @brief The audio board to use
    audio_board_handle_t audio_board;
} recorder_config_t;

#endif //RECORDER_CONFIG_H
