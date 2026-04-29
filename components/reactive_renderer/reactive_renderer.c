#include "reactive_renderer.h"
#include "audio_processor.h"
#include "led_driver.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <math.h>

static const char *TAG = "rx_render";

static reactive_effect_t s_effect = REACTIVE_EFFECT_VU_BAR;
static app_config_t s_cfg;
static bool s_cfg_ready = false;

static struct {
    uint32_t frame;
    uint16_t chase_idx;
    int16_t  comet_pos;
    bool     comet_active;
    uint8_t  flash_phase;
    uint8_t  bass_glow;
    uint8_t  beat_glow;
    uint8_t  vu_peak;
    uint8_t  spectrum_peak[AUDIO_SPECTRUM_BANDS_MAX];
} RS;

static uint8_t sc8(uint8_t v, uint8_t s) { return (uint8_t)(((uint16_t)v * s) / 255); }

static led_color_t dim_c(led_color_t c, uint8_t d)
{
    return (led_color_t){sc8(c.r,d), sc8(c.g,d), sc8(c.b,d), sc8(c.w,d)};
}

static led_color_t blend_c(led_color_t a, led_color_t b, uint8_t t)
{
    return (led_color_t){
        sc8(a.r,255-t)+sc8(b.r,t), sc8(a.g,255-t)+sc8(b.g,t),
        sc8(a.b,255-t)+sc8(b.b,t), sc8(a.w,255-t)+sc8(b.w,t)
    };
}

static led_color_t add_c(led_color_t a, led_color_t b)
{
    uint16_t r = (uint16_t)a.r + b.r;
    uint16_t g = (uint16_t)a.g + b.g;
    uint16_t bl = (uint16_t)a.b + b.b;
    uint16_t w = (uint16_t)a.w + b.w;
    return (led_color_t){r > 255 ? 255 : r, g > 255 ? 255 : g,
                         bl > 255 ? 255 : bl, w > 255 ? 255 : w};
}

static void hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    while (h >= 360) h -= 360;
    while (h < 0) h += 360;
    int i = (int)(h / 60);
    float f = h/60 - i, p = v*(1-s), q = v*(1-s*f), t2 = v*(1-s*(1-f));
    float rf, gf, bf;
    switch (i) {
        case 0: rf=v; gf=t2; bf=p; break;
        case 1: rf=q; gf=v;  bf=p; break;
        case 2: rf=p; gf=v;  bf=t2; break;
        case 3: rf=p; gf=q;  bf=v; break;
        case 4: rf=t2; gf=p; bf=v; break;
        default: rf=v; gf=p; bf=q; break;
    }
    *r = (uint8_t)(rf*255); *g = (uint8_t)(gf*255); *b = (uint8_t)(bf*255);
}

static led_color_t hsv_c(float h, float s, float v)
{
    uint8_t r, g, b;
    hsv2rgb(h, s, v, &r, &g, &b);
    return (led_color_t){r, g, b, 0};
}

static void cfg_or_default(app_config_t *out)
{
    if (s_cfg_ready) { *out = s_cfg; return; }
    config_manager_load(out);
    s_cfg = *out;
    s_cfg_ready = true;
}

/* ===== EFFECTS ===== */

/* VU_BAR — mirrored center meter with peak hold and beat glow */
static void fx_vu_bar(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    uint16_t half = n / 2;
    uint16_t bar = (uint32_t)a->volume_8bit * half / 255;
    if (bar > half) bar = half;
    if (a->volume_8bit > RS.vu_peak) RS.vu_peak = a->volume_8bit;
    else if ((RS.frame & 1) == 0 && RS.vu_peak > 2) RS.vu_peak -= 2;
    if (a->beat_detected) RS.beat_glow = 255;
    else if (RS.beat_glow > 12) RS.beat_glow -= 12;
    else RS.beat_glow = 0;

    for (uint16_t i = 0; i < n; i++) {
        int32_t d = (int32_t)i - (int32_t)half;
        if (d < 0) d = -d;
        if ((uint16_t)d <= bar) {
            uint8_t t = bar > 0 ? (uint8_t)((uint32_t)d * 255 / bar) : 0;
            led_color_t c = blend_c(pri, sec, t);
            if (RS.beat_glow) c = add_c(c, dim_c(sec, RS.beat_glow / 3));
            led_driver_set_pixel(i, c);
        } else {
            uint8_t halo = RS.beat_glow > d ? (uint8_t)(RS.beat_glow - d) : 0;
            led_driver_set_pixel(i, halo ? add_c(dim_c(bg, 80), dim_c(pri, halo / 4)) : bg);
        }
    }

    uint16_t peak = (uint32_t)RS.vu_peak * half / 255;
    if (peak > 0 && peak < half) {
        led_color_t peak_c = add_c(sec, (led_color_t){96, 96, 96, 0});
        led_driver_set_pixel(half + peak, peak_c);
        led_driver_set_pixel(half - peak, peak_c);
    }
}

/* PULSE — soft wave field modulated by volume, with beat flash */
static void fx_pulse(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};

    uint8_t v = a->volume_8bit;
    if (a->beat_detected) RS.beat_glow = 255;
    else if (RS.beat_glow > 8) RS.beat_glow -= 8;
    else RS.beat_glow = 0;

    uint8_t bright = cfg.brightness;
    uint16_t boost = (uint16_t)bright + (uint16_t)v * (cfg.fade_amount ? cfg.fade_amount : 40) / 255;
    bright = boost > 255 ? 255 : (uint8_t)boost;
    led_driver_set_brightness(bright);

    uint16_t wave_speed = 3 + ((uint16_t)cfg.animation_speed * 9 / 255);
    uint16_t phase = (uint16_t)(RS.frame * wave_speed);
    for (uint16_t i = 0; i < n; i++) {
        uint8_t sweep = (uint8_t)(((uint32_t)i * 255 / (n ? n : 1) + phase) & 0xff);
        uint8_t band = sweep < 128 ? sweep * 2 : (255 - sweep) * 2;
        uint8_t mix = (uint8_t)(((uint16_t)band + v) / 2);
        led_color_t c = blend_c(bg, blend_c(pri, sec, band), mix);
        if (RS.beat_glow) c = add_c(c, dim_c(sec, RS.beat_glow / 2));
        led_driver_set_pixel(i, c);
    }
}

/* BEAT_FLASH — impact flash that decays into a moving afterglow */
static void fx_beat_flash(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    if (a->beat_detected) RS.flash_phase = 12;
    if (RS.flash_phase > 0) {
        uint8_t energy = (uint8_t)((uint16_t)RS.flash_phase * 255 / 12);
        for (uint16_t i = 0; i < n; i++) {
            uint8_t wave = (uint8_t)(((uint32_t)i * 255 / (n ? n : 1) + RS.frame * 18) & 0xff);
            uint8_t edge = wave < 128 ? wave * 2 : (255 - wave) * 2;
            led_color_t c = blend_c(pri, sec, edge);
            led_driver_set_pixel(i, blend_c(bg, c, energy));
        }
        RS.flash_phase--;
    } else {
        uint8_t idle = a->volume_8bit / 5;
        for (uint16_t i = 0; i < n; i++) {
            uint8_t p = (uint8_t)((i + RS.frame) & 0x1f);
            led_driver_set_pixel(i, p < 2 ? dim_c(sec, idle + 16) : bg);
        }
    }
}

/* SPARK — audio-triggered glitter with bass-tinted low bands and random stars */
static void fx_spark(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    uint8_t bass_floor = a->bass_level / 5;
    for (uint16_t i = 0; i < n; i++) led_driver_set_pixel(i, dim_c(bg, bass_floor));

    if (a->volume_8bit > 10) {
        uint8_t dens = cfg.density ? cfg.density : 30;
        uint16_t sparks = 1 + ((uint32_t)(a->volume_8bit + a->treble_level) * dens * n) / (255 * 255);
        if (a->beat_detected) sparks += n / 18 + 1;
        if (sparks > n) sparks = n;
        for (uint16_t j = 0; j < sparks; j++) {
            uint16_t pos = esp_random() % n;
            led_color_t c;
            uint32_t r = esp_random() % 4;
            if (r == 0) c = hsv_c((float)((RS.frame * 7 + pos * 3) % 360), 1, 1);
            else if (r == 1) c = pri;
            else if (r == 2) c = sec;
            else c = add_c(pri, dim_c(sec, 120));
            led_driver_set_pixel(pos, c);
            if (pos > 0) led_driver_set_pixel(pos - 1, dim_c(c, 90));
            if (pos + 1 < n) led_driver_set_pixel(pos + 1, dim_c(c, 70));
        }
    }
}

/* COMET — beat-triggered dual comet with audio-reactive tail */
static void fx_comet(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    uint8_t tl   = cfg.tail_length ? cfg.tail_length : 12;
    uint8_t fade = cfg.fade_amount ? cfg.fade_amount : 60;
    uint8_t speed = 1 + (uint8_t)((uint16_t)(cfg.animation_speed + a->volume_8bit) / 96);

    if ((a->onset_detected || a->beat_detected) && !RS.comet_active) {
        RS.comet_active = true;
        RS.comet_pos = 0;
    }
    for (uint16_t i = 0; i < n; i++) led_driver_set_pixel(i, dim_c(bg, 70));
    if (RS.comet_active) {
        for (uint8_t j = 0; j < tl; j++) {
            int16_t p = RS.comet_pos - j;
            if (p >= 0 && p < (int16_t)n) {
                uint8_t f = 255 - (j * fade / tl);
                led_color_t c = j == 0 ? add_c(pri, dim_c(sec, a->treble_level / 2)) : dim_c(blend_c(pri, sec, j * 255 / tl), f);
                led_driver_set_pixel((uint16_t)p, c);
            }
            int16_t rp = (int16_t)n - 1 - p;
            if (rp >= 0 && rp < (int16_t)n) {
                uint8_t f = 255 - (j * fade / tl);
                led_driver_set_pixel((uint16_t)rp, dim_c(blend_c(sec, pri, j * 255 / tl), f));
            }
        }
        RS.comet_pos += speed;
        if (RS.comet_pos >= (int16_t)n + tl) RS.comet_active = false;
    } else if (a->volume_8bit > 18) {
        uint16_t pos = ((uint32_t)RS.frame * speed) % n;
        led_driver_set_pixel(pos, dim_c(sec, a->volume_8bit));
    }
}

/* CHASE — multi-segment chase whose speed and spacing follow audio */
static void fx_chase(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    uint8_t base = cfg.animation_speed ? cfg.animation_speed : 80;
    uint16_t eff = (uint16_t)base + a->volume_8bit / 2;
    if (eff > 255) eff = 255;
    uint32_t step = 1 + ((uint32_t)(255 - eff) * 10) / 255;
    if (RS.frame % step == 0) RS.chase_idx = (RS.chase_idx + 1) % n;

    uint8_t sz   = cfg.animation_size ? cfg.animation_size : 3;
    sz += a->mid_level / 72;
    uint8_t fade = cfg.fade_amount ? cfg.fade_amount : 40;
    uint8_t lanes = a->beat_detected ? 4 : 3;
    for (uint16_t i = 0; i < n; i++) {
        led_color_t out = dim_c(bg, 80);
        for (uint8_t lane = 0; lane < lanes; lane++) {
            uint16_t head = (RS.chase_idx + (uint32_t)lane * n / lanes) % n;
            int32_t d = (int32_t)head - (int32_t)i;
            if (d < 0) d += n;
            if (d < sz) {
                uint8_t f = 255 - (uint8_t)(d * fade / sz);
                led_color_t c = dim_c(lane & 1 ? sec : pri, f);
                out = add_c(out, c);
            }
        }
        led_driver_set_pixel(i, out);
    }
}

/* SPECTRUM_BARS — segmented rainbow analyzer with peak hold */
static void fx_spectrum(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t bg  = {cfg.bg_r, cfg.bg_g, cfg.bg_b, cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    uint8_t bands = a->band_count ? a->band_count : 16;
    if (bands > AUDIO_SPECTRUM_BANDS_MAX) bands = AUDIO_SPECTRUM_BANDS_MAX;
    if (bands == 0) { led_driver_set_all(bg); return; }

    uint16_t per = n / bands;
    if (per == 0) per = 1;

    for (uint8_t b = 0; b < bands; b++) {
        uint8_t mag = a->spectrum_bands[b];
        if (mag > RS.spectrum_peak[b]) RS.spectrum_peak[b] = mag;
        else if ((RS.frame & 3) == 0 && RS.spectrum_peak[b] > 4) RS.spectrum_peak[b] -= 4;
        else if (RS.spectrum_peak[b] <= 4) RS.spectrum_peak[b] = 0;
    }

    for (uint16_t i = 0; i < n; i++) {
        uint8_t band = i / per;
        if (band >= bands) band = bands - 1;
        uint8_t mag  = a->spectrum_bands[band];
        uint16_t local = i - band * per;
        uint16_t band_h = (uint32_t)mag * per / 255;
        if (local < band_h) {
            float hue = fmodf((float)band * (360.0f / bands) + (float)(RS.frame % 180), 360.0f);
            float val = 0.35f + ((float)local / (float)(per ? per : 1)) * 0.65f;
            led_driver_set_pixel(i, hsv_c(hue, 1.0f, val));
        } else {
            uint16_t peak_h = (uint32_t)RS.spectrum_peak[band] * per / 255;
            if (peak_h > 0 && local == peak_h) {
                led_driver_set_pixel(i, (led_color_t){255, 230, 160, 0});
            } else {
                led_driver_set_pixel(i, bg);
            }
        }
    }
}

/* BASS_HIT — center bass bloom with side accents */
static void fx_bass_hit(const audio_features_t *a, uint16_t n)
{
    app_config_t cfg; cfg_or_default(&cfg);
    led_color_t pri = {cfg.color_r, cfg.color_g, cfg.color_b, cfg.color_w};
    led_color_t sec = {cfg.sec_r,   cfg.sec_g,   cfg.sec_b,   cfg.sec_w};
    led_color_t bg  = {cfg.bg_r,    cfg.bg_g,    cfg.bg_b,    cfg.bg_w};
    led_driver_set_brightness(cfg.brightness);

    if (a->bass_level > RS.bass_glow) RS.bass_glow = a->bass_level;
    else if (RS.bass_glow > 6) RS.bass_glow -= 6;
    else RS.bass_glow = 0;

    uint16_t span = (uint32_t)RS.bass_glow * n / 255;
    uint16_t mid = n / 2;
    uint16_t half = span / 2;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t d = i > mid ? i - mid : mid - i;
        bool inside = d <= half;
        if (inside) {
            uint8_t f = half > 0 ? (uint8_t)(255 - ((uint32_t)d * 180 / half)) : 255;
            led_driver_set_pixel(i, dim_c(blend_c(pri, sec, f / 2), (uint8_t)(((uint16_t)RS.bass_glow + f) / 2)));
        } else {
            uint8_t ripple = (uint8_t)((d + RS.frame * 2) & 0x3f);
            led_driver_set_pixel(i, ripple < 4 ? dim_c(sec, a->mid_level / 2) : bg);
        }
    }

    if (a->beat_detected && n > 8) {
        for (uint8_t j = 0; j < 4; j++) {
            uint16_t p = (uint16_t)(((uint32_t)j * n / 4 + RS.frame) % n);
            led_driver_set_pixel(p, add_c(pri, sec));
        }
    }
}

typedef void (*rx_fn_t)(const audio_features_t *a, uint16_t n);

static const rx_fn_t k_fx[REACTIVE_EFFECT_MAX] = {
    [REACTIVE_EFFECT_VU_BAR]        = fx_vu_bar,
    [REACTIVE_EFFECT_PULSE]         = fx_pulse,
    [REACTIVE_EFFECT_BEAT_FLASH]    = fx_beat_flash,
    [REACTIVE_EFFECT_SPARK]         = fx_spark,
    [REACTIVE_EFFECT_COMET]         = fx_comet,
    [REACTIVE_EFFECT_CHASE]         = fx_chase,
    [REACTIVE_EFFECT_SPECTRUM_BARS] = fx_spectrum,
    [REACTIVE_EFFECT_BASS_HIT]      = fx_bass_hit,
};

esp_err_t reactive_renderer_init(void)
{
    memset(&RS, 0, sizeof(RS));
    s_effect = REACTIVE_EFFECT_VU_BAR;
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t reactive_renderer_set_effect(reactive_effect_t effect)
{
    if ((int)effect < 0 || (int)effect >= REACTIVE_EFFECT_MAX) return ESP_ERR_INVALID_ARG;
    if (effect != s_effect) {
        s_effect = effect;
        memset(&RS, 0, sizeof(RS));
        ESP_LOGI(TAG, "effect=%s", config_manager_reactive_effect_to_string(effect));
    }
    return ESP_OK;
}

reactive_effect_t reactive_renderer_get_effect(void) { return s_effect; }

esp_err_t reactive_renderer_update(uint32_t delta_ms)
{
    audio_features_t a;
    if (audio_processor_get_features(&a) != ESP_OK) memset(&a, 0, sizeof(a));

    uint16_t n = led_driver_get_count();
    if (n == 0) return ESP_OK;

    if (s_cfg_ready && !s_cfg.power) {
        led_driver_clear();
        led_driver_show();
        return ESP_OK;
    }

    rx_fn_t fn = k_fx[s_effect] ? k_fx[s_effect] : fx_vu_bar;
    fn(&a, n);
    led_driver_show();
    RS.frame++;
    return ESP_OK;
}

esp_err_t reactive_renderer_reset(void)
{
    memset(&RS, 0, sizeof(RS));
    return ESP_OK;
}

esp_err_t reactive_renderer_set_params(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    s_cfg_ready = true;
    return ESP_OK;
}
