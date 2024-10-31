#ifndef CONFIG_H
#define CONFIG_H
#include <board.h>
#include <i2s_stream.h>
#include <string>

/// @brief Retrieves a i2s stream config with sane defaults
i2s_stream_cfg_t get_i2s_stream_cfg(int core, int sample_rate) noexcept;

/// @brief Configures the audio board and DAC with sane defaults
void configure_audio_board(const audio_board_handle_t& board_handle) noexcept;

/// @brief Retrieves a timestamped filename
std::string get_filename() noexcept;

#endif //CONFIG_H
