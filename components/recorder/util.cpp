#include "util.h"

#include <board_def.h>
#include <es8388.h>
#include <format>
#include <time_util.h>


i2s_stream_cfg_t get_i2s_stream_cfg(const int core, const int sample_rate) noexcept {
    return {
            .type = AUDIO_STREAM_READER,
            .transmit_mode = I2S_COMM_MODE_STD,
            .chan_cfg = {
                .id = static_cast<i2s_port_t>(CODEC_ADC_I2S_PORT),
                .role = I2S_ROLE_MASTER,
                .dma_desc_num = 3,
                .dma_frame_num = 312,
                .auto_clear = true,
            },
            .std_cfg = {
                .clk_cfg = {
                    .sample_rate_hz = static_cast<uint32_t>(sample_rate),
                    .clk_src = I2S_CLK_SRC_DEFAULT,
                    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                },
                .slot_cfg = {
                    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                    .slot_mode = I2S_SLOT_MODE_MONO,
                    .slot_mask = I2S_STD_SLOT_LEFT,
                    .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                    .ws_pol = false,
                    .bit_shift = true,
                    .msb_right = (I2S_DATA_BIT_WIDTH_16BIT <= I2S_DATA_BIT_WIDTH_16BIT) ? true : false, },
                    .gpio_cfg = {
                    .invert_flags = { .mclk_inv = false, .bclk_inv = false, },
                },
        },
        .expand_src_bits = I2S_DATA_BIT_WIDTH_16BIT,
        .use_alc = false,
        .volume = 0,
        .out_rb_size = (8 * 1024),
        .task_stack = (3584),
        .task_core = core,
        .task_prio = (23),
        .stack_in_ext = false,
        .multi_out_num = 1,
        .uninstall_drv = true,
        .need_expand = false,
        .buffer_len = (3600),
    };
}

void configure_audio_board(const audio_board_handle_t &board_handle) noexcept {
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 100);

    // Write to the registers
    es8388_write_reg(ES8388_ADCCONTROL10, 0b11111010);
    es8388_write_reg(ES8388_ADCCONTROL2, ADC_INPUT_LINPUT2_RINPUT2);
    es8388_write_reg(ES8388_ADCCONTROL8, 0b01111000); //Reg16 - LADCVOL Attenuation
    es8388_write_reg(ES8388_ADCCONTROL9, 0b01111000); //Reg17 - RADCVOL Attenuation
    es8388_write_reg(ES8388_ADCCONTROL14, 0b01011000);
}

std::string get_filename() noexcept {
    return std::format("{0}.wav", get_time());
}

