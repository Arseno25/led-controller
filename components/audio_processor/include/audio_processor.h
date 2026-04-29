#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t raw_level;
    uint16_t rms_level;
    uint16_t peak_level;
    uint16_t smoothed_level;
    uint16_t signal_level;
    uint16_t noise_floor;
    int16_t dc_offset;
    uint16_t clipped_samples;

    uint8_t volume_8bit;       /* normalized 0..255 */
    uint8_t bass_level;        /* 0..255 */
    uint8_t mid_level;
    uint8_t treble_level;

    bool beat_detected;
    bool onset_detected;

    float dominant_frequency;
    float spectral_centroid;

    uint8_t spectrum_bands[AUDIO_SPECTRUM_BANDS_MAX];
    uint8_t band_count;

    uint16_t auto_gain_x100;   /* applied gain * 100 */
    uint16_t auto_gate_level;
} audio_features_t;

esp_err_t audio_processor_init(const audio_config_t *cfg);
esp_err_t audio_processor_set_config(const audio_config_t *cfg);
esp_err_t audio_processor_process(const int16_t *samples, size_t count, audio_features_t *out);
esp_err_t audio_processor_get_features(audio_features_t *out);

#ifdef __cplusplus
}
#endif
