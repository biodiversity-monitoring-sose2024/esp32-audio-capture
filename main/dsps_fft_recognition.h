//
// Created by Tristan  Wesendahl Velázquez on 16.09.24.
//

#ifndef FFT_ANALYSIS_HPP
#define FFT_ANALYSIS_HPP

#include <esp_dsp.h>
#include <audio_pipeline.h>
#include "audio_element.h"
#include <cstring>


audio_element_handle_t FFTAnalyser_init();
extern bool birdDetected;  


#endif // FFT_ANALYSIS_HPP

