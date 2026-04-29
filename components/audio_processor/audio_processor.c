#include "audio_processor.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "audio_proc";

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static SemaphoreHandle_t s_mux = NULL;
static audio_config_t   s_cfg;
static audio_features_t s_feat;

static float    s_smoothed = 0.0f;
static float    s_band_smoothed[AUDIO_SPECTRUM_BANDS_MAX];
static float    s_agc = 1.0f;
static float    s_energy_avg = 0.0f;
static float    s_dc_offset = 0.0f;
static float    s_noise_floor = 35.0f;
static uint32_t s_last_beat_ms = 0;
static int16_t *s_work = NULL;
static size_t   s_work_cap = 0;

static const float k_band_centers[AUDIO_SPECTRUM_BANDS_MAX] = {
    60, 100, 160, 250, 400, 630, 1000, 1600,
    2200, 2800, 3400, 4000, 4600, 5200, 5800, 6400
};

static inline uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static uint8_t clamp_u8(float v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static uint16_t clamp_u16(float v)
{
    if (v < 0) return 0;
    if (v > 65535) return 65535;
    return (uint16_t)v;
}

static esp_err_t ensure_work(size_t n)
{
    if (n <= s_work_cap) return ESP_OK;
    int16_t *p = (int16_t *)realloc(s_work, n * sizeof(int16_t));
    if (!p) return ESP_ERR_NO_MEM;
    s_work = p;
    s_work_cap = n;
    return ESP_OK;
}

esp_err_t audio_processor_init(const audio_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (!s_mux) {
        s_mux = xSemaphoreCreateMutex();
        if (!s_mux) return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_cfg = *cfg;
    memset(&s_feat, 0, sizeof(s_feat));
    memset(s_band_smoothed, 0, sizeof(s_band_smoothed));
    s_smoothed = 0;
    s_agc = 1.0f;
    s_energy_avg = 0;
    s_dc_offset = 0.0f;
    s_noise_floor = cfg->noise_gate > 0 ? (float)cfg->noise_gate : 35.0f;
    xSemaphoreGive(s_mux);
    ESP_LOGI(TAG, "init bands=%u sens=%u smth=%u",
             cfg->spectrum_bands, cfg->sensitivity, cfg->smoothing);
    return ESP_OK;
}

esp_err_t audio_processor_set_config(const audio_config_t *cfg)
{
    if (!cfg || !s_mux) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_cfg = *cfg;
    if (s_noise_floor < 4.0f || cfg->noise_gate < s_noise_floor * 0.5f) {
        s_noise_floor = cfg->noise_gate > 0 ? (float)cfg->noise_gate : 35.0f;
    }
    xSemaphoreGive(s_mux);
    return ESP_OK;
}

static float goertzel_mag2(const int16_t *s, size_t n, float target_hz, float rate)
{
    float k = 0.5f + ((float)n * target_hz / rate);
    float w = 2.0f * (float)M_PI * k / (float)n;
    float cosw = cosf(w);
    float coeff = 2.0f * cosw;
    float q0, q1 = 0, q2 = 0;
    for (size_t i = 0; i < n; i++) {
        q0 = coeff * q1 - q2 + (float)s[i];
        q2 = q1;
        q1 = q0;
    }
    return q1*q1 + q2*q2 - q1*q2*coeff;
}

esp_err_t audio_processor_process(const int16_t *samples, size_t count, audio_features_t *out)
{
    if (!samples || count == 0 || !s_mux) return ESP_ERR_INVALID_ARG;

    audio_config_t cfg;
    xSemaphoreTake(s_mux, portMAX_DELAY);
    cfg = s_cfg;
    xSemaphoreGive(s_mux);

    size_t n = count;
    if (cfg.fft_size > 0 && cfg.fft_size < n) n = cfg.fft_size;
    if (n == 0) return ESP_ERR_INVALID_ARG;
    esp_err_t err = ensure_work(n);
    if (err != ESP_OK) return err;

    /* DC removal + high-pass conditioning before RMS/spectrum. */
    uint64_t sumsq = 0;
    int32_t  peak = 0;
    uint16_t clipped = 0;
    for (size_t i = 0; i < n; i++) {
        float raw = (float)samples[i];
        s_dc_offset += (raw - s_dc_offset) * 0.0015f;
        int32_t hp = (int32_t)(raw - s_dc_offset);
        if (hp > INT16_MAX) { hp = INT16_MAX; clipped++; }
        if (hp < INT16_MIN) { hp = INT16_MIN; clipped++; }
        s_work[i] = (int16_t)hp;
        int32_t v = hp; if (v < 0) v = -v;
        if (v > peak) peak = v;
        sumsq += (uint64_t)(hp * hp);
    }
    float rms = sqrtf((float)sumsq / (float)n);

    /*
     * Adaptive floor makes INMP441 usable across quiet and noisy rooms.
     * It tracks quiet input quickly downward, but rises slowly so music does
     * not get learned as noise.
     */
    if (rms > 0.0f) {
        float floor_alpha = rms < s_noise_floor ? 0.08f : 0.0025f;
        if (rms < s_noise_floor * 1.8f || s_noise_floor < 5.0f) {
            s_noise_floor += (rms - s_noise_floor) * floor_alpha;
        }
        if (s_noise_floor < 4.0f) s_noise_floor = 4.0f;
    }

    float adaptive_gate = s_noise_floor * 1.20f + 6.0f;
    float manual_gate = (float)cfg.noise_gate;
    float gate = manual_gate > adaptive_gate ? manual_gate : adaptive_gate;
    float signal = rms > gate ? (rms - gate) : 0.0f;

    /* Manual gain */
    float manual_gain = (float)cfg.gain / 100.0f;
    float gained = signal * manual_gain;

    /* Auto gain */
    if (cfg.auto_gain) {
        if (gained > 1.0f) {
            float target_peak = 12000.0f;
            float ratio = target_peak / gained;
            s_agc += (ratio - s_agc) * 0.02f;
            if (s_agc < 0.35f) s_agc = 0.35f;
            if (s_agc > 16.0f) s_agc = 16.0f;
        }
        gained *= s_agc;
    }

    /* Sensitivity */
    float sens = (float)cfg.sensitivity / 70.0f;
    gained *= sens;

    /* Smoothing */
    float alpha = (float)(100 - cfg.smoothing) / 100.0f;
    if (alpha < 0.05f) alpha = 0.05f;
    s_smoothed += (gained - s_smoothed) * alpha;

    uint16_t raw_u16  = clamp_u16(rms);
    uint16_t peak_u16 = clamp_u16(peak);
    uint16_t smth_u16 = clamp_u16(s_smoothed);
    uint16_t sig_u16  = clamp_u16(signal);

    uint8_t vol8 = clamp_u8(s_smoothed / 24.0f);

    uint8_t bands = cfg.spectrum_bands;
    if (bands == 0) bands = 1;
    if (bands > AUDIO_SPECTRUM_BANDS_MAX) bands = AUDIO_SPECTRUM_BANDS_MAX;

    float band_norm = 1.0f / (float)n;
    float band_alpha = alpha;
    float bass = 0, mid = 0, treble = 0;
    int   bass_n = 0, mid_n = 0, treble_n = 0;
    float dominant_mag = 0; float dominant_hz = 0;
    float centroid_num = 0, centroid_den = 0;

    if (cfg.fft_enabled) {
        float nyq = (float)cfg.sample_rate * 0.5f;
        for (uint8_t b = 0; b < bands; b++) {
            float hz = k_band_centers[b];
            if (hz >= nyq) { s_band_smoothed[b] = 0; s_feat.spectrum_bands[b] = 0; continue; }

            float m2 = goertzel_mag2(s_work, n, hz, (float)cfg.sample_rate) * band_norm;
            float mag = sqrtf(m2 < 0 ? 0 : m2);

            if (cfg.bass_boost && hz < 250)    mag *= 1.4f;
            if (cfg.treble_boost && hz > 3000) mag *= 1.4f;

            mag *= s_agc * sens;

            s_band_smoothed[b] += (mag - s_band_smoothed[b]) * band_alpha;
            uint8_t u = clamp_u8(s_band_smoothed[b] / 24.0f);

            if (hz <= 250)       { bass   += u; bass_n++; }
            else if (hz <= 2000) { mid    += u; mid_n++;  }
            else                 { treble += u; treble_n++; }

            if (mag > dominant_mag) { dominant_mag = mag; dominant_hz = hz; }
            centroid_num += hz * mag;
            centroid_den += mag;

            s_feat.spectrum_bands[b] = u;
        }
        for (uint8_t b = bands; b < AUDIO_SPECTRUM_BANDS_MAX; b++) {
            s_feat.spectrum_bands[b] = 0;
        }
    } else {
        memset(s_feat.spectrum_bands, vol8, sizeof(s_feat.spectrum_bands));
    }

    /* Beat detection */
    float energy = s_smoothed;
    s_energy_avg += (energy - s_energy_avg) * 0.05f;
    float beat_factor = (float)cfg.beat_threshold / 500.0f;
    if (beat_factor < 1.05f) beat_factor = 1.05f;
    bool beat = false;
    uint32_t t_now = now_ms();
    if (energy > s_energy_avg * beat_factor && energy > 200.0f) {
        if ((t_now - s_last_beat_ms) > 120) {
            beat = true;
            s_last_beat_ms = t_now;
        }
    }
    bool onset = (energy > s_energy_avg * (beat_factor + 0.4f) && energy > 400.0f);

    xSemaphoreTake(s_mux, portMAX_DELAY);
    s_feat.raw_level      = raw_u16;
    s_feat.rms_level      = raw_u16;
    s_feat.peak_level     = peak_u16;
    s_feat.smoothed_level = smth_u16;
    s_feat.signal_level   = sig_u16;
    s_feat.noise_floor    = clamp_u16(s_noise_floor);
    s_feat.dc_offset      = (int16_t)s_dc_offset;
    s_feat.clipped_samples = clipped;
    s_feat.volume_8bit    = vol8;
    s_feat.bass_level     = clamp_u8(bass_n   ? bass   / bass_n   : 0);
    s_feat.mid_level      = clamp_u8(mid_n    ? mid    / mid_n    : 0);
    s_feat.treble_level   = clamp_u8(treble_n ? treble / treble_n : 0);
    s_feat.beat_detected  = beat;
    s_feat.onset_detected = onset;
    s_feat.dominant_frequency = dominant_hz;
    s_feat.spectral_centroid  = (centroid_den > 0) ? (centroid_num / centroid_den) : 0;
    s_feat.band_count     = bands;
    s_feat.auto_gain_x100 = (uint16_t)(s_agc * 100.0f);
    s_feat.auto_gate_level = clamp_u16(gate);
    if (out) *out = s_feat;
    xSemaphoreGive(s_mux);

    return ESP_OK;
}

esp_err_t audio_processor_get_features(audio_features_t *out)
{
    if (!out || !s_mux) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mux, portMAX_DELAY);
    *out = s_feat;
    xSemaphoreGive(s_mux);
    return ESP_OK;
}
