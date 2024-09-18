#include "audio_recorder.h"
#include "util.h"
#include <esp_log.h>
#include <es8388.h>

void AudioRecorder::start()
{
    this->board_handle = audio_board_init();
    audio_hal_ctrl_codec(this->board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(this->board_handle->audio_hal, 100);
    es8388_write_reg(ES8388_ADCCONTROL1, 0b01110111);
    es8388_write_reg(ES8388_ADCCONTROL10, 0b11111110);
    es8388_write_reg(ES8388_ADCCONTROL2, ADC_INPUT_LINPUT2_RINPUT2);

    this->i2s_config = { 
        .type = AUDIO_STREAM_READER, 
        .transmit_mode = I2S_COMM_MODE_STD, 
        .chan_cfg = { 
            .id = (i2s_port_t)CODEC_ADC_I2S_PORT, 
            .role = I2S_ROLE_MASTER, 
            .dma_desc_num = 3, 
            .dma_frame_num = 312, 
            .auto_clear = true, 
        }, 
        .std_cfg = { 
            .clk_cfg = { 
                .sample_rate_hz = this->config.sample_rate,
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
        .task_core = this->config.core, 
        .task_prio = (23), 
        .stack_in_ext = false, 
        .multi_out_num = 1, 
        .uninstall_drv = true, 
        .need_expand = false, 
        .buffer_len = (3600), 
    };

    this->wav_config = DEFAULT_WAV_ENCODER_CONFIG();
    this->wav_encoder = wav_encoder_init(&this->wav_config);

    this->fatfs_config = FATFS_STREAM_CFG_DEFAULT();
    this->fatfs_config.type = AUDIO_STREAM_WRITER;
    this->fatfs_config.task_core = 1;

    this->wav_fatfs_stream_writer = fatfs_stream_init(&this->fatfs_config);
    this->i2s_stream_reader = i2s_stream_init(&this->i2s_config);
    i2s_stream_set_clk(this->i2s_stream_reader, this->config.sample_rate, 16, 1);
    
    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(this->i2s_stream_reader, &info);
    audio_element_getinfo(this->wav_fatfs_stream_writer, &info);
    info.bits = 16;
    info.sample_rates = this->config.sample_rate;
    info.channels = 1;
    audio_element_setinfo(this->i2s_stream_reader, &info);
    audio_element_setinfo(this->wav_fatfs_stream_writer, &info);

    this->rb_i2s_to_wav = rb_create(this->config.buffer_size, 1);
    audio_element_set_input_ringbuf(this->wav_encoder, this->rb_i2s_to_wav);
    audio_element_set_output_ringbuf(this->i2s_stream_reader, this->rb_i2s_to_wav);

    this->rb_wav_to_fatstream = rb_create(this->config.buffer_size, 1);
    audio_element_set_input_ringbuf(this->wav_fatfs_stream_writer, this->rb_wav_to_fatstream);
    audio_element_set_output_ringbuf(this->wav_encoder, this->rb_wav_to_fatstream);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    this->pipeline = audio_pipeline_init(&pipeline_cfg);

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
        audio_pipeline_register(this->pipeline, this->i2s_stream_reader, "reader");
        audio_pipeline_register(this->pipeline, this->wav_encoder, "encoder");
        audio_pipeline_register(this->pipeline, this->wav_fatfs_stream_writer, "writer");
        const char* link_tag[3] = {"reader", "encoder", "writer"};
        audio_pipeline_link(this->pipeline, &link_tag[0], 3);
        std::string filename = this->config.record_path + "/" + std::to_string(get_time()) + ".wav";
        audio_element_set_uri(this->wav_fatfs_stream_writer, filename.c_str());        
        audio_pipeline_run(this->pipeline);

        ESP_LOGI(this->TAG.c_str(), "[ * ] Recording %s...", filename.c_str()); 
        std::this_thread::sleep_for(39s);
        
        audio_pipeline_stop(this->pipeline);
        audio_pipeline_wait_for_stop(this->pipeline);
        audio_pipeline_terminate(this->pipeline);

        ESP_LOGI(this->TAG.c_str(), "[ * ] Sending %s...", filename.c_str());  
        auto thread = this->client.start_file_transfer(filename);
        thread->detach();
        delete thread;
    }
}
