#include <cmath>
#include "esp_dsp.h"
#include <audio_pipeline.h>
#include "audio_element.h"
#include <cstring>
#include "esp_log.h"


constexpr int SAMPLE_RATE = 48000;
constexpr int FREQUENZ_MIN = 1500;
constexpr int FREQUENZ_MAX = 10000;
constexpr float THRESHOLD = 0.15f;

static const char *TAG = "FFTProcessor";

bool birdDetected = false;


class FFTProcessor {
public:
    FFTProcessor() {
        // Initialisierung der FFT
        if (dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE) != ESP_OK) {
            initialized = false;
        } else {
            initialized = true;
        }
    }

    bool analyze(const float input[], int N_SAMPLES) {
        if (!initialized) {
            ESP_LOGE(TAG, "FFTProcessor not initialized.");
            return false;
        }

        // ESP_LOGI(TAG, "Starting FFT analysis with %d samples.", N_SAMPLES);


        // ESP_LOGI(TAG, "Input data (first 10 samples):");
        // for (int i = 0; i < 10 && i < N_SAMPLES; ++i) {
        //     ESP_LOGI(TAG, "input[%d]: %.6f", i, input[i]);
        // }       

        float* wind = (float*)malloc(N_SAMPLES * sizeof(float));
        float* y_cf = (float*)malloc(N_SAMPLES * 2 * sizeof(float));
        // float averageAmplitude = 0;
        float highestAmplitude = 0;
        bool found = false;


        // Hann-Fenster erstellen
        dsps_wind_hann_f32(wind, N_SAMPLES);
        // ESP_LOGI(TAG, "Hann window created.");

        // Eingangsvektor in komplexen Vektor umwandeln
        for (int i = 0; i < N_SAMPLES; ++i) {
            y_cf[i * 2] = input[i] * wind[i];
            y_cf[i * 2 + 1] = 0.0f;  // Imaginary Teil auf 0 setzen
        }
        free(wind);
        // ESP_LOGI(TAG, "Input data transformed to complex vector.");

        // FFT anwenden
        dsps_fft2r_fc32(y_cf, N_SAMPLES);
        dsps_bit_rev_fc32(y_cf, N_SAMPLES);
        dsps_cplx2reC_fc32(y_cf, N_SAMPLES);
        // ESP_LOGI(TAG, "FFT applied successfully.");

        // Frequenzspektrum analysieren
        for (int i = 0; i < N_SAMPLES / 2; ++i) {  
            float real_part = y_cf[i * 2];
            float frequency = (i * SAMPLE_RATE) / N_SAMPLES;

            if (frequency >= FREQUENZ_MIN && frequency <= FREQUENZ_MAX) {
                float amplitude = std::fabs(real_part);

                if (highestAmplitude < amplitude) {
                    highestAmplitude = amplitude;
                }

                // averageAmplitude += amplitude;
                // ESP_LOGI(TAG, "Frequency analysed: %.2f Hz with amplitude: %.2f", frequency, amplitude);
                if (amplitude > THRESHOLD) {
                    // ESP_LOGI(TAG, "Frequency detected: %.2f Hz with amplitude: %.2f", frequency, amplitude);
                    if (!birdDetected) {
                        ESP_LOGI(TAG, "Frequency detected: %.2f Hz with amplitude: %.2f", frequency, amplitude);
                    }
                    found = true;
                }
            }
        }

        // ESP_LOGI(TAG, "No significant frequencies detected.");
        ESP_LOGI(TAG, "Highest amplitude with amplitude: %.2f", highestAmplitude);
        free(y_cf);
        return found;
    }


private:
    bool initialized;
};


struct FFTContext {
    FFTProcessor* processor;   // Der FFT-Processor
    float* floatFFTBuf;        // Der konvertierte Float-Puffer für die Daten
    int buffer_len;            // Pufferlänge (für Speichermanagement)
};



static audio_element_err_t fft_process(audio_element_handle_t self, char *inbuf, int len) {
    // ESP_LOGI(TAG, "Starting FFT process with input buffer length: %d", len);

    FFTContext* ctx = (FFTContext*)audio_element_getdata(self);
    if (!ctx || !ctx->processor || !ctx->floatFFTBuf) {
        ESP_LOGE(TAG, "Failed to retrieve context or allocate memory.");
        return AEL_PROCESS_FAIL;
    }

    audio_element_input(self, inbuf, len);

    

    // ESP_LOGI(TAG, "Raw buffer data (first 10 bytes):");
    // for (int i = 0; i < 10 && i < len; ++i) {
    //     ESP_LOGI(TAG, "buffer[%d]: %02X", i, inbuf[i]);
    // }


    // Teil 1: Lesen und Konvertieren der Daten (aus fft_read)
    int16_t* sample_data = reinterpret_cast<int16_t*>(inbuf);
    int N_SAMPLES = len / 2;

    // Log der PCM-Daten
    // ESP_LOGI(TAG, "PCM data (first 10 samples):");
    // for (int i = 0; i < 10 && i < N_SAMPLES; ++i) {
    //     ESP_LOGI(TAG, "sample_data[%d]: %d", i, sample_data[i]);
    // }

    // Konvertiere PCM-Daten in Float
    for (int i = 0; i < N_SAMPLES; ++i) {
        ctx->floatFFTBuf[i] = static_cast<float>(sample_data[i]) / 32768.0f;
    }

    ctx->buffer_len = N_SAMPLES;  // Aktualisiere die Puffergröße
    // ESP_LOGI(TAG, "Converted %d PCM samples to float.", N_SAMPLES);

    // Teil 2: FFT-Analyse durchführen
    bool bird_detected = ctx->processor->analyze(ctx->floatFFTBuf, ctx->buffer_len);
    // ESP_LOGI(TAG, "Bird detection result: %s", bird_detected ? "detected" : "not_detected");

    audio_element_set_tag(self, bird_detected ? "detected" : "not_detected");

    // Teil 3: Schreiboperation basierend auf Tag (aus fft_write)
    const char *tag = audio_element_get_tag(self);
    // ESP_LOGI(TAG, "Current detection tag: %s", tag);

    if (!birdDetected && strcmp(tag, "detected") == 0) {
        ESP_LOGI(TAG, "Bird detected. Writing output buffer.");
         // Daten nur ausgeben, wenn Vogel erkannt wurde
        birdDetected = true;
    } else {
        // ESP_LOGI(TAG, "No bird detected. Skipping write.");
    }

     return audio_element_output(self, inbuf, len);
}


static esp_err_t fft_open(audio_element_handle_t self) {
    ESP_LOGI(TAG, "Opening FFT processor.");
    birdDetected = false;

    FFTContext* ctx = (FFTContext*)malloc(sizeof(FFTContext));
    if (!ctx) {
        ESP_LOGE(TAG, "Failed to allocate memory for FFT context.");
        return ESP_FAIL;
    }

    ctx->processor = new FFTProcessor();
    if (!ctx->processor) {
        ESP_LOGE(TAG, "Failed to initialize FFTProcessor.");
        free(ctx);
        return ESP_FAIL;
    }

    ctx->buffer_len = 2048;
    ctx->floatFFTBuf = (float*)malloc(ctx->buffer_len * sizeof(float));
    if (!ctx->floatFFTBuf) {
        ESP_LOGE(TAG, "Failed to allocate memory for float buffer.");
        delete ctx->processor;
        free(ctx);
        return ESP_FAIL;
    }

    audio_element_setdata(self, ctx);
    ESP_LOGI(TAG, "FFT processor opened successfully.");
    return ESP_OK;
}


static esp_err_t fft_close(audio_element_handle_t self) {
    ESP_LOGI(TAG, "Closing FFT processor.");

    FFTContext* ctx = (FFTContext*)audio_element_getdata(self);
    if (ctx) {
        if (ctx->floatFFTBuf) {
            free(ctx->floatFFTBuf);
            ctx->floatFFTBuf = NULL;
            ESP_LOGI(TAG, "Float buffer freed.");
        }
    }

    ESP_LOGI(TAG, "FFT processor closed successfully.");
    return ESP_OK;
}


static esp_err_t fft_destroy(audio_element_handle_t self) {
    ESP_LOGI(TAG, "Destroying FFT processor and context.");

    FFTContext* ctx = (FFTContext*)audio_element_getdata(self);  // Holen des Kontexts
    if (ctx) {
        if (ctx->processor) {
            ESP_LOGI(TAG, "Deleting FFT processor.");
            delete ctx->processor;  // Lösche den FFTProcessor
        } else {
            ESP_LOGW(TAG, "FFT processor was already NULL.");
        }

        ESP_LOGI(TAG, "Deleting FFT context.");
        delete ctx;  // Gib den gesamten Kontext frei
    } else {
        ESP_LOGW(TAG, "FFT context is already NULL.");
    }

    ESP_LOGI(TAG, "FFT processor and context destroyed successfully.");
    return ESP_OK;
}


audio_element_handle_t FFTAnalyser_init() // no equalizer array to pass in !
    {
    audio_element_cfg_t FFTAnalyserCfg; // = DEFAULT_AUDIO_ELEMENT_CONFIG();
    memset(&FFTAnalyserCfg, 0, sizeof(audio_element_cfg_t));
    FFTAnalyserCfg.destroy = fft_destroy;
    FFTAnalyserCfg.process = fft_process;
    FFTAnalyserCfg.open = fft_open;
    FFTAnalyserCfg.close = fft_close;
    FFTAnalyserCfg.buffer_len = (4*1024);
    FFTAnalyserCfg.tag = "fft_filter";
    FFTAnalyserCfg.task_stack = (8 * 1024);
    FFTAnalyserCfg.task_prio = (5);
    FFTAnalyserCfg.task_core = (0); 
    FFTAnalyserCfg.out_rb_size = (8 * 1024);

    audio_element_handle_t FFTAnalyseProcessor = audio_element_init(&FFTAnalyserCfg);
    return FFTAnalyseProcessor;
}







// static audio_element_err_t fft_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context) {
//     audio_element_input(self, buffer, len);
//     FFTContext* ctx = (FFTContext*)audio_element_getdata(self);  // Holen des Kontexts

//     if (!ctx || !ctx->floatFFTBuf) {
//         ESP_LOGE(TAG, "Context or float buffer not initialized.");
//         return AEL_IO_FAIL;  // Fehler bei Speicherallokation
//     }

//     ESP_LOGI(TAG, "Reading %d bytes from input buffer.", len);

//     int16_t* sample_data = reinterpret_cast<int16_t*>(buffer);
//     int N_SAMPLES = len / 2;

//     // Konvertiere PCM-Daten in Float
//     for (int i = 0; i < N_SAMPLES; ++i) {
//         ctx->floatFFTBuf[i] = static_cast<float>(sample_data[i]) / 32768.0f;
//     }

//     ctx->buffer_len = N_SAMPLES;  // Aktualisiere die Puffergröße
//     ESP_LOGI(TAG, "Converted %d PCM samples to float.", N_SAMPLES);

//     return AEL_IO_OK;  // Erfolgreicher Lesevorgang
// }



// static audio_element_err_t fft_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context) {
//     const char *tag = audio_element_get_tag(self);

//     ESP_LOGI(TAG, "Writing %d bytes to output buffer.", len);
//     ESP_LOGI(TAG, "Current detection tag: %s", tag);

//     // Schreibe nur, wenn Vögel erkannt wurden
//     if (strcmp(tag, "detected") == 0) {
//         ESP_LOGI(TAG, "Bird detected. Writing output buffer.");
//         return audio_element_output(self, buffer, len);
//     } else {
//         ESP_LOGI(TAG, "No bird detected. Skipping write.");
//     }

//     return AEL_IO_OK;  // Erfolgreicher Schreibvorgang, auch wenn nichts geschrieben wurde
// }


