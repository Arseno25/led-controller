#include "config_manager.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "config_mgr";
#define NVS_NAMESPACE "pixel_config"

/* --- NVS keys (max 15 chars) --- */
#define KEY_DEVICE_NAME "dev_name"
#define KEY_OP_MODE     "op_mode"

#define KEY_LED_TYPE    "led_type"
#define KEY_LED_PIN     "led_pin"
#define KEY_LED_COUNT   "led_count"
#define KEY_BRIGHTNESS  "brightness"
#define KEY_POWER       "power"
#define KEY_COLOR_R     "color_r"
#define KEY_COLOR_G     "color_g"
#define KEY_COLOR_B     "color_b"
#define KEY_COLOR_W     "color_w"
#define KEY_SEC_R       "sec_r"
#define KEY_SEC_G       "sec_g"
#define KEY_SEC_B       "sec_b"
#define KEY_SEC_W       "sec_w"
#define KEY_BG_R        "bg_r"
#define KEY_BG_G        "bg_g"
#define KEY_BG_B        "bg_b"
#define KEY_BG_W        "bg_w"

#define KEY_ANIMATION   "anim"
#define KEY_ANIM_SPEED  "anim_spd"
#define KEY_ANIM_DIR    "anim_dir"
#define KEY_ANIM_SIZE   "anim_size"
#define KEY_TAIL_LEN    "tail_len"
#define KEY_FADE_AMT    "fade_amt"
#define KEY_DENSITY     "density"
#define KEY_INTENSITY   "intensity"
#define KEY_COOLING     "cooling"
#define KEY_SPARKING    "sparking"
#define KEY_ANIM_LOOP   "anim_loop"
#define KEY_MIRROR      "mirror"
#define KEY_RAND_COLOR  "rand_color"
#define KEY_CUST_PAT    "cust_pat"

#define KEY_RX_EFFECT   "rx_effect"
#define KEY_MX_EFFECT   "mx_effect"
#define KEY_RX_MX_EFFE  "rx_mx_eff"
#define KEY_PALETTE     "palette"

/* audio */
#define KEY_A_BCLK      "a_bclk"
#define KEY_A_WS        "a_ws"
#define KEY_A_DATA      "a_data"
#define KEY_A_RATE      "a_rate"
#define KEY_A_BUF       "a_buf"
#define KEY_A_FFT_SZ    "a_fft_sz"
#define KEY_A_SENS      "a_sens"
#define KEY_A_GAIN      "a_gain"
#define KEY_A_AGC       "a_agc"
#define KEY_A_NGATE     "a_ngate"
#define KEY_A_SMTH      "a_smth"
#define KEY_A_BEAT_TH   "a_beat_th"
#define KEY_A_FFT_EN    "a_fft_en"
#define KEY_A_BANDS     "a_bands"
#define KEY_A_BBOOST    "a_bboost"
#define KEY_A_TBOOST    "a_tboost"

/* matrix */
#define KEY_M_EN        "m_en"
#define KEY_M_W         "m_w"
#define KEY_M_H         "m_h"
#define KEY_M_LAYOUT    "m_layout"
#define KEY_M_ORIGIN    "m_origin"
#define KEY_M_RX        "m_rx"
#define KEY_M_RY        "m_ry"
#define KEY_M_ROT90     "m_rot90"

/* random reactive */
#define KEY_RR_EN       "rr_en"
#define KEY_RR_INTV     "rr_intv"
#define KEY_RR_NOREP    "rr_norep"
#define KEY_RR_INC_S    "rr_inc_s"
#define KEY_RR_INC_M    "rr_inc_m"
#define KEY_RR_FAVS     "rr_favs"

/* random normal */
#define KEY_RN_EN       "rn_en"
#define KEY_RN_INTV     "rn_intv"
#define KEY_RN_NOREP    "rn_norep"
#define KEY_RN_FAVS     "rn_favs"

/* display */
#define KEY_D_EN        "d_en"
#define KEY_D_BRIGHT    "d_bright"
#define KEY_D_THEME     "d_theme"
#define KEY_D_VIEW      "d_view"
#define KEY_D_FPS       "d_fps"
#define KEY_D_WIFI      "d_wifi"

static bool gpio_pin_valid(int8_t pin, bool output)
{
    if (pin < 0) return false;
    return output ? GPIO_IS_VALID_OUTPUT_GPIO(pin) : GPIO_IS_VALID_GPIO(pin);
}

static const char *k_led_type_names[LED_TYPE_MAX] = {
    [LED_TYPE_WS2812B]="WS2812B",[LED_TYPE_WS2811]="WS2811",
    [LED_TYPE_WS2813]="WS2813",[LED_TYPE_WS2815]="WS2815",
    [LED_TYPE_SK6812_RGB]="SK6812_RGB",[LED_TYPE_SK6812_RGBW]="SK6812_RGBW",
    [LED_TYPE_APA102]="APA102",[LED_TYPE_SK9822]="SK9822",
    [LED_TYPE_NEON_PIXEL_5V]="NEON_PIXEL_5V",
    [LED_TYPE_NEON_PIXEL_12V]="NEON_PIXEL_12V",
};

static const char *k_animation_names[ANIM_TYPE_MAX] = {
    [ANIM_SOLID]="solid",[ANIM_BLINK]="blink",[ANIM_BREATHING]="breathing",
    [ANIM_RAINBOW]="rainbow",[ANIM_COLOR_WIPE]="color_wipe",[ANIM_RUNNING_DOT]="running_dot",
    [ANIM_COMET]="comet",[ANIM_METEOR_RAIN]="meteor_rain",[ANIM_THEATER_CHASE]="theater_chase",
    [ANIM_SCANNER]="scanner",[ANIM_TWINKLE]="twinkle",[ANIM_SPARKLE]="sparkle",
    [ANIM_FIRE]="fire",[ANIM_POLICE_LIGHT]="police_light",[ANIM_NEON_FLICKER]="neon_flicker",
    [ANIM_GRADIENT_FLOW]="gradient_flow",[ANIM_WAVE]="wave",[ANIM_CONFETTI]="confetti",
    [ANIM_PULSE]="pulse",[ANIM_CUSTOM]="custom",
};

static const char *k_direction_names[ANIM_DIR_MAX] = {
    [ANIM_DIR_FORWARD]="forward",[ANIM_DIR_REVERSE]="reverse",[ANIM_DIR_BOUNCE]="bounce",
};

static const char *k_custom_pattern_names[CUSTOM_PATTERN_MAX] = {
    [CUSTOM_PATTERN_DOT]="dot",[CUSTOM_PATTERN_BAR]="bar",[CUSTOM_PATTERN_GRADIENT]="gradient",
    [CUSTOM_PATTERN_WAVE]="wave",[CUSTOM_PATTERN_RANDOM_SPARK]="random_spark",
    [CUSTOM_PATTERN_DUAL_COLOR_CHASE]="dual_color_chase",
};

static const char *k_operating_mode_names[OPERATING_MODE_MAX] = {
    [OPERATING_MODE_NORMAL]="normal",
    [OPERATING_MODE_REACTIVE]="reactive",
    [OPERATING_MODE_MATRIX]="matrix",
    [OPERATING_MODE_REACTIVE_MATRIX]="reactive_matrix",
};

static const char *k_reactive_effect_names[REACTIVE_EFFECT_MAX] = {
    [REACTIVE_EFFECT_VU_BAR]="reactive_vu_bar",
    [REACTIVE_EFFECT_PULSE]="reactive_pulse",
    [REACTIVE_EFFECT_BEAT_FLASH]="reactive_beat_flash",
    [REACTIVE_EFFECT_SPARK]="reactive_spark",
    [REACTIVE_EFFECT_COMET]="reactive_comet",
    [REACTIVE_EFFECT_CHASE]="reactive_chase",
    [REACTIVE_EFFECT_SPECTRUM_BARS]="reactive_spectrum_bars",
    [REACTIVE_EFFECT_BASS_HIT]="reactive_bass_hit",
};

static const char *k_matrix_effect_names[MATRIX_EFFECT_MAX] = {
    [MATRIX_EFFECT_RAIN]="matrix_rain",
    [MATRIX_EFFECT_RAINBOW]="matrix_rainbow",
    [MATRIX_EFFECT_FIRE]="matrix_fire",
    [MATRIX_EFFECT_PLASMA]="matrix_plasma",
    [MATRIX_EFFECT_LAVA]="matrix_lava",
    [MATRIX_EFFECT_AURORA]="matrix_aurora",
    [MATRIX_EFFECT_RIPPLE]="matrix_ripple",
    [MATRIX_EFFECT_STARFIELD]="matrix_starfield",
    [MATRIX_EFFECT_WATERFALL]="matrix_waterfall",
    [MATRIX_EFFECT_BLOCKS]="matrix_blocks",
    [MATRIX_EFFECT_NOISE_FLOW]="matrix_noise_flow",
    [MATRIX_EFFECT_SPIRAL]="matrix_spiral",
};

static const char *k_rx_matrix_effect_names[RX_MATRIX_MAX] = {
    [RX_MATRIX_SPECTRUM_BARS]="matrix_spectrum_bars",
    [RX_MATRIX_CENTER_VU]="matrix_center_vu",
    [RX_MATRIX_BASS_PULSE]="matrix_bass_pulse",
    [RX_MATRIX_AUDIO_RIPPLE]="matrix_audio_ripple",
    [RX_MATRIX_BEAT_FLASH]="matrix_beat_flash",
    [RX_MATRIX_SPARK_FIELD]="matrix_spark_field",
    [RX_MATRIX_AUDIO_PLASMA]="matrix_audio_plasma",
    [RX_MATRIX_FIRE_EQ]="matrix_fire_eq",
    [RX_MATRIX_BASS_TUNNEL]="matrix_bass_tunnel",
    [RX_MATRIX_TREBLE_RAIN]="matrix_treble_rain",
    [RX_MATRIX_MID_WAVE]="matrix_mid_wave",
    [RX_MATRIX_AUDIO_BLOCKS]="matrix_audio_blocks",
    [RX_MATRIX_REACTIVE_AURORA]="matrix_reactive_aurora",
    [RX_MATRIX_REACTIVE_SPIRAL]="matrix_reactive_spiral",
    [RX_MATRIX_SPECTRUM_RAINBOW]="matrix_spectrum_rainbow",
    [RX_MATRIX_SPECTRUM_MIRROR]="matrix_spectrum_mirror",
    [RX_MATRIX_SPECTRUM_PEAKS]="matrix_spectrum_peaks",
    [RX_MATRIX_SPECTRUM_WATERFALL]="matrix_spectrum_waterfall",
    [RX_MATRIX_SEGMENT_VU]="matrix_segment_vu",
};

static const char *k_matrix_layout_names[MATRIX_LAYOUT_MAX] = {
    [MATRIX_LAYOUT_SERPENTINE]="serpentine",
    [MATRIX_LAYOUT_PROGRESSIVE]="progressive",
};

static const char *k_matrix_origin_names[MATRIX_ORIGIN_MAX] = {
    [MATRIX_ORIGIN_TOP_LEFT]="top_left",
    [MATRIX_ORIGIN_TOP_RIGHT]="top_right",
    [MATRIX_ORIGIN_BOTTOM_LEFT]="bottom_left",
    [MATRIX_ORIGIN_BOTTOM_RIGHT]="bottom_right",
};

static const char *k_display_theme_names[DISPLAY_THEME_MAX] = {
    [DISPLAY_THEME_NEON_DARK]="neon_dark",
};

static const char *k_display_view_names[DISPLAY_VIEW_MAX] = {
    [DISPLAY_VIEW_AUTO]="auto",
    [DISPLAY_VIEW_STATUS]="status",
    [DISPLAY_VIEW_SPECTRUM]="spectrum",
    [DISPLAY_VIEW_VU_METER]="vu_meter",
    [DISPLAY_VIEW_WAVEFORM]="waveform",
};

#define ENUM_HELPERS(prefix, type, names, count, fallback) \
bool prefix##_is_valid(type v) { return (int)v >= 0 && (int)v < (int)(count); } \
const char *prefix##_to_string(type v) { return prefix##_is_valid(v) ? names[v] : fallback; } \
esp_err_t prefix##_from_string(const char *s, type *o) { \
    if (!s||!o) return ESP_ERR_INVALID_ARG; \
    for (int i=0;i<(int)(count);i++) { if (strcmp(s,names[i])==0) { *o=(type)i; return ESP_OK; } } \
    return ESP_ERR_NOT_FOUND; }

bool config_manager_led_pin_is_safe(uint8_t pin) {
    return gpio_pin_valid((int8_t)pin, true);
}
bool config_manager_led_type_is_valid(led_type_t t) { return t>=0 && t<LED_TYPE_MAX; }
const char *config_manager_led_type_to_string(led_type_t t) { return config_manager_led_type_is_valid(t)?k_led_type_names[t]:"UNKNOWN"; }
esp_err_t config_manager_led_type_from_string(const char *s, led_type_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<LED_TYPE_MAX;i++) { if (strcmp(s,k_led_type_names[i])==0) { *o=(led_type_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

ENUM_HELPERS(config_manager_animation, animation_type_t, k_animation_names, ANIM_TYPE_MAX, "unknown")
ENUM_HELPERS(config_manager_direction, animation_direction_t, k_direction_names, ANIM_DIR_MAX, "forward")
ENUM_HELPERS(config_manager_custom_pattern, custom_pattern_type_t, k_custom_pattern_names, CUSTOM_PATTERN_MAX, "dot")

const char *config_manager_operating_mode_to_string(operating_mode_t m) {
    return ((int)m>=0 && (int)m<OPERATING_MODE_MAX) ? k_operating_mode_names[m] : "normal";
}
esp_err_t config_manager_operating_mode_from_string(const char *s, operating_mode_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<OPERATING_MODE_MAX;i++) { if (strcmp(s,k_operating_mode_names[i])==0) { *o=(operating_mode_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_reactive_effect_to_string(reactive_effect_t e) {
    return ((int)e>=0 && (int)e<REACTIVE_EFFECT_MAX) ? k_reactive_effect_names[e] : "reactive_vu_bar";
}
esp_err_t config_manager_reactive_effect_from_string(const char *s, reactive_effect_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<REACTIVE_EFFECT_MAX;i++) { if (strcmp(s,k_reactive_effect_names[i])==0) { *o=(reactive_effect_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_matrix_effect_to_string(matrix_effect_t e) {
    return ((int)e>=0 && (int)e<MATRIX_EFFECT_MAX) ? k_matrix_effect_names[e] : "matrix_rain";
}
esp_err_t config_manager_matrix_effect_from_string(const char *s, matrix_effect_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<MATRIX_EFFECT_MAX;i++) { if (strcmp(s,k_matrix_effect_names[i])==0) { *o=(matrix_effect_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_rx_matrix_effect_to_string(rx_matrix_effect_t e) {
    return ((int)e>=0 && (int)e<RX_MATRIX_MAX) ? k_rx_matrix_effect_names[e] : "matrix_spectrum_bars";
}
esp_err_t config_manager_rx_matrix_effect_from_string(const char *s, rx_matrix_effect_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<RX_MATRIX_MAX;i++) { if (strcmp(s,k_rx_matrix_effect_names[i])==0) { *o=(rx_matrix_effect_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_matrix_layout_to_string(matrix_layout_t l) {
    return ((int)l>=0 && (int)l<MATRIX_LAYOUT_MAX) ? k_matrix_layout_names[l] : "serpentine";
}
esp_err_t config_manager_matrix_layout_from_string(const char *s, matrix_layout_t *o) {
    if (!s||!o) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<MATRIX_LAYOUT_MAX;i++) { if (strcmp(s,k_matrix_layout_names[i])==0) { *o=(matrix_layout_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_matrix_origin_to_string(matrix_origin_t o) {
    return ((int)o>=0 && (int)o<MATRIX_ORIGIN_MAX) ? k_matrix_origin_names[o] : "top_left";
}
esp_err_t config_manager_matrix_origin_from_string(const char *s, matrix_origin_t *out) {
    if (!s||!out) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<MATRIX_ORIGIN_MAX;i++) { if (strcmp(s,k_matrix_origin_names[i])==0) { *out=(matrix_origin_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_display_theme_to_string(display_theme_t t) {
    return ((int)t>=0 && (int)t<DISPLAY_THEME_MAX) ? k_display_theme_names[t] : "neon_dark";
}
esp_err_t config_manager_display_theme_from_string(const char *s, display_theme_t *out) {
    if (!s||!out) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<DISPLAY_THEME_MAX;i++) { if (strcmp(s,k_display_theme_names[i])==0) { *out=(display_theme_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

const char *config_manager_display_view_to_string(display_view_mode_t v) {
    return ((int)v>=0 && (int)v<DISPLAY_VIEW_MAX) ? k_display_view_names[v] : "auto";
}
esp_err_t config_manager_display_view_from_string(const char *s, display_view_mode_t *out) {
    if (!s||!out) return ESP_ERR_INVALID_ARG;
    for (int i=0;i<DISPLAY_VIEW_MAX;i++) { if (strcmp(s,k_display_view_names[i])==0) { *out=(display_view_mode_t)i; return ESP_OK; } }
    return ESP_ERR_NOT_FOUND;
}

void config_manager_default(app_config_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    strncpy(out->device_name, DEFAULT_DEVICE_NAME, CONFIG_DEVICE_NAME_MAX_LEN-1);

    out->operating_mode = OPERATING_MODE_NORMAL;

    out->led_type=DEFAULT_LED_TYPE; out->led_pin=DEFAULT_LED_PIN;
    out->led_count=DEFAULT_LED_COUNT; out->brightness=DEFAULT_BRIGHTNESS;
    out->power=DEFAULT_POWER;
    out->color_r=DEFAULT_COLOR_R; out->color_g=DEFAULT_COLOR_G;
    out->color_b=DEFAULT_COLOR_B; out->color_w=DEFAULT_COLOR_W;
    out->animation=DEFAULT_ANIMATION; out->animation_speed=DEFAULT_ANIMATION_SPEED;
    out->animation_direction=DEFAULT_ANIM_DIRECTION;
    out->animation_size=DEFAULT_ANIM_SIZE; out->tail_length=DEFAULT_TAIL_LENGTH;
    out->fade_amount=DEFAULT_FADE_AMOUNT; out->density=DEFAULT_DENSITY;
    out->intensity=DEFAULT_INTENSITY; out->cooling=DEFAULT_COOLING;
    out->sparking=DEFAULT_SPARKING;
    out->anim_loop=true; out->mirror=false; out->random_color=false;
    out->custom_pattern=DEFAULT_CUSTOM_PATTERN;

    out->active_reactive_effect = REACTIVE_EFFECT_VU_BAR;
    out->active_matrix_effect   = MATRIX_EFFECT_RAIN;
    out->active_rx_matrix_effect= RX_MATRIX_SPECTRUM_BARS;
    out->palette_id = 0;

    audio_config_t *a = &out->audio;
    a->i2s_bclk_pin = DEFAULT_I2S_BCLK_PIN;
    a->i2s_ws_pin   = DEFAULT_I2S_WS_PIN;
    a->i2s_data_pin = DEFAULT_I2S_DATA_PIN;
    a->sample_rate  = DEFAULT_AUDIO_SAMPLE_RATE;
    a->buffer_size  = DEFAULT_AUDIO_BUFFER_SIZE;
    a->fft_size     = DEFAULT_AUDIO_FFT_SIZE;
    a->sensitivity  = DEFAULT_AUDIO_SENSITIVITY;
    a->gain         = DEFAULT_AUDIO_GAIN;
    a->auto_gain    = DEFAULT_AUDIO_AUTO_GAIN;
    a->noise_gate   = DEFAULT_AUDIO_NOISE_GATE;
    a->smoothing    = DEFAULT_AUDIO_SMOOTHING;
    a->beat_threshold = DEFAULT_AUDIO_BEAT_THRESHOLD;
    a->fft_enabled  = DEFAULT_AUDIO_FFT_ENABLED;
    a->spectrum_bands = DEFAULT_AUDIO_SPECTRUM_BANDS;
    a->bass_boost = false; a->treble_boost = false;

    matrix_config_t *m = &out->matrix;
    m->enabled = DEFAULT_MATRIX_ENABLED;
    m->width = DEFAULT_MATRIX_WIDTH;
    m->height= DEFAULT_MATRIX_HEIGHT;
    m->layout= MATRIX_LAYOUT_SERPENTINE;
    m->origin= MATRIX_ORIGIN_TOP_LEFT;
    m->reverse_x=false; m->reverse_y=false; m->rotate_90=false;

    out->random_reactive.enabled = false;
    out->random_reactive.interval_seconds = DEFAULT_RANDOM_INTERVAL;
    out->random_reactive.no_repeat = true;
    out->random_reactive.include_strip = true;
    out->random_reactive.include_matrix= true;
    out->random_reactive.only_favorites=false;

    out->random_normal.enabled = false;
    out->random_normal.interval_seconds = DEFAULT_RANDOM_INTERVAL;
    out->random_normal.no_repeat = true;
    out->random_normal.only_favorites=false;

    out->display.enabled = DEFAULT_DISPLAY_ENABLED;
    out->display.brightness = DEFAULT_DISPLAY_BRIGHTNESS;
    out->display.theme = DISPLAY_THEME_NEON_DARK;
    out->display.view_mode = DISPLAY_VIEW_AUTO;
    out->display.show_fps = true;
    out->display.show_wifi = true;
}

esp_err_t config_manager_validate(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (!config_manager_led_type_is_valid(cfg->led_type)) return ESP_ERR_INVALID_ARG;
    if (!config_manager_led_pin_is_safe(cfg->led_pin)) return ESP_ERR_INVALID_ARG;
    if (cfg->led_count<CONFIG_LED_COUNT_MIN||cfg->led_count>CONFIG_LED_COUNT_MAX) return ESP_ERR_INVALID_ARG;
    if (!config_manager_animation_is_valid(cfg->animation)) return ESP_ERR_INVALID_ARG;
    if (cfg->animation_speed<1) return ESP_ERR_INVALID_ARG;
    if (!config_manager_direction_is_valid(cfg->animation_direction)) return ESP_ERR_INVALID_ARG;
    if ((int)cfg->operating_mode < 0 || (int)cfg->operating_mode >= OPERATING_MODE_MAX) return ESP_ERR_INVALID_ARG;
    if ((int)cfg->active_reactive_effect < 0 || (int)cfg->active_reactive_effect >= REACTIVE_EFFECT_MAX) return ESP_ERR_INVALID_ARG;
    if ((int)cfg->active_matrix_effect < 0 || (int)cfg->active_matrix_effect >= MATRIX_EFFECT_MAX) return ESP_ERR_INVALID_ARG;
    if ((int)cfg->active_rx_matrix_effect < 0 || (int)cfg->active_rx_matrix_effect >= RX_MATRIX_MAX) return ESP_ERR_INVALID_ARG;

    /* audio bounds */
    const audio_config_t *a = &cfg->audio;
    if (!gpio_pin_valid(a->i2s_bclk_pin, true)) return ESP_ERR_INVALID_ARG;
    if (!gpio_pin_valid(a->i2s_ws_pin, true)) return ESP_ERR_INVALID_ARG;
    if (!gpio_pin_valid(a->i2s_data_pin, false)) return ESP_ERR_INVALID_ARG;
    if (a->sample_rate < 8000 || a->sample_rate > 48000) return ESP_ERR_INVALID_ARG;
    if (a->buffer_size < 64 || a->buffer_size > 2048) return ESP_ERR_INVALID_ARG;
    if (a->fft_size < 64 || a->fft_size > 2048) return ESP_ERR_INVALID_ARG;
    if (a->sensitivity == 0 || a->gain == 0) return ESP_ERR_INVALID_ARG;
    if (a->smoothing > 100) return ESP_ERR_INVALID_ARG;
    if (a->spectrum_bands == 0 || a->spectrum_bands > AUDIO_SPECTRUM_BANDS_MAX) return ESP_ERR_INVALID_ARG;

    /* matrix bounds */
    const matrix_config_t *m = &cfg->matrix;
    if (m->width == 0 || m->height == 0) return ESP_ERR_INVALID_ARG;
    if (m->width > MATRIX_MAX_W || m->height > MATRIX_MAX_H) return ESP_ERR_INVALID_ARG;

    /* random */
    if (cfg->random_reactive.interval_seconds < 5) return ESP_ERR_INVALID_ARG;
    if (cfg->random_normal.interval_seconds < 5) return ESP_ERR_INVALID_ARG;

    if ((int)cfg->display.theme < 0 || (int)cfg->display.theme >= DISPLAY_THEME_MAX) return ESP_ERR_INVALID_ARG;
    if ((int)cfg->display.view_mode < 0 || (int)cfg->display.view_mode >= DISPLAY_VIEW_MAX) return ESP_ERR_INVALID_ARG;

    return ESP_OK;
}

esp_err_t config_manager_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err==ESP_ERR_NVS_NO_FREE_PAGES||err==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG,"nvs erase"); ESP_ERROR_CHECK(nvs_flash_erase()); err=nvs_flash_init();
    }
    if (err!=ESP_OK) return err;
    return ESP_OK;
}

static esp_err_t rd_u8 (nvs_handle_t h,const char*k,uint8_t fb,uint8_t*o){
    esp_err_t e=nvs_get_u8(h,k,o); if(e==ESP_ERR_NVS_NOT_FOUND){*o=fb;return ESP_OK;} return e;}
static esp_err_t rd_u16(nvs_handle_t h,const char*k,uint16_t fb,uint16_t*o){
    esp_err_t e=nvs_get_u16(h,k,o); if(e==ESP_ERR_NVS_NOT_FOUND){*o=fb;return ESP_OK;} return e;}
static esp_err_t rd_u32(nvs_handle_t h,const char*k,uint32_t fb,uint32_t*o){
    esp_err_t e=nvs_get_u32(h,k,o); if(e==ESP_ERR_NVS_NOT_FOUND){*o=fb;return ESP_OK;} return e;}
static esp_err_t rd_i8 (nvs_handle_t h,const char*k,int8_t fb,int8_t*o){
    esp_err_t e=nvs_get_i8(h,k,o); if(e==ESP_ERR_NVS_NOT_FOUND){*o=fb;return ESP_OK;} return e;}

esp_err_t config_manager_load(app_config_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    config_manager_default(out);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err==ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err!=ESP_OK) return err;

    size_t nl=sizeof(out->device_name);
    err=nvs_get_str(h,KEY_DEVICE_NAME,out->device_name,&nl);
    if (err==ESP_ERR_NVS_NOT_FOUND) { strncpy(out->device_name,DEFAULT_DEVICE_NAME,sizeof(out->device_name)-1); err=ESP_OK; }

    uint8_t u8;
    #define RU8(key,fb,dst) if(err==ESP_OK){err=rd_u8(h,key,fb,&u8);dst=(typeof(dst))u8;}
    #define RU8D(key,fb,dst) if(err==ESP_OK) err=rd_u8(h,key,fb,&dst)
    #define RBOOL(key,fb,dst) if(err==ESP_OK){err=rd_u8(h,key,(fb)?1:0,&u8);dst=(u8!=0);}
    #define RU16(key,fb,dst) if(err==ESP_OK) err=rd_u16(h,key,fb,&dst)
    #define RU32(key,fb,dst) if(err==ESP_OK) err=rd_u32(h,key,fb,&dst)

    RU8(KEY_OP_MODE, (uint8_t)OPERATING_MODE_NORMAL, out->operating_mode);
    RU8(KEY_LED_TYPE,(uint8_t)DEFAULT_LED_TYPE,out->led_type);
    RU8D(KEY_LED_PIN,DEFAULT_LED_PIN,out->led_pin);
    RU16(KEY_LED_COUNT,DEFAULT_LED_COUNT,out->led_count);
    RU8D(KEY_BRIGHTNESS,DEFAULT_BRIGHTNESS,out->brightness);
    RBOOL(KEY_POWER,1,out->power);

    RU8D(KEY_COLOR_R,DEFAULT_COLOR_R,out->color_r); RU8D(KEY_COLOR_G,DEFAULT_COLOR_G,out->color_g);
    RU8D(KEY_COLOR_B,DEFAULT_COLOR_B,out->color_b); RU8D(KEY_COLOR_W,DEFAULT_COLOR_W,out->color_w);
    RU8D(KEY_SEC_R,0,out->sec_r); RU8D(KEY_SEC_G,0,out->sec_g);
    RU8D(KEY_SEC_B,0,out->sec_b); RU8D(KEY_SEC_W,0,out->sec_w);
    RU8D(KEY_BG_R,0,out->bg_r); RU8D(KEY_BG_G,0,out->bg_g);
    RU8D(KEY_BG_B,0,out->bg_b); RU8D(KEY_BG_W,0,out->bg_w);

    RU8(KEY_ANIMATION,(uint8_t)DEFAULT_ANIMATION,out->animation);
    RU8D(KEY_ANIM_SPEED,DEFAULT_ANIMATION_SPEED,out->animation_speed);
    RU8(KEY_ANIM_DIR,(uint8_t)DEFAULT_ANIM_DIRECTION,out->animation_direction);
    RU8D(KEY_ANIM_SIZE,DEFAULT_ANIM_SIZE,out->animation_size);
    RU8D(KEY_TAIL_LEN,DEFAULT_TAIL_LENGTH,out->tail_length);
    RU8D(KEY_FADE_AMT,DEFAULT_FADE_AMOUNT,out->fade_amount);
    RU8D(KEY_DENSITY,DEFAULT_DENSITY,out->density);
    RU8D(KEY_INTENSITY,DEFAULT_INTENSITY,out->intensity);
    RU8D(KEY_COOLING,DEFAULT_COOLING,out->cooling);
    RU8D(KEY_SPARKING,DEFAULT_SPARKING,out->sparking);
    RBOOL(KEY_ANIM_LOOP,1,out->anim_loop);
    RBOOL(KEY_MIRROR,0,out->mirror);
    RBOOL(KEY_RAND_COLOR,0,out->random_color);
    RU8(KEY_CUST_PAT,(uint8_t)DEFAULT_CUSTOM_PATTERN,out->custom_pattern);

    RU8(KEY_RX_EFFECT,(uint8_t)REACTIVE_EFFECT_VU_BAR,out->active_reactive_effect);
    RU8(KEY_MX_EFFECT,(uint8_t)MATRIX_EFFECT_RAIN,out->active_matrix_effect);
    RU8(KEY_RX_MX_EFFE,(uint8_t)RX_MATRIX_SPECTRUM_BARS,out->active_rx_matrix_effect);
    RU8D(KEY_PALETTE,0,out->palette_id);

    /* audio */
    audio_config_t *a = &out->audio;
    if(err==ESP_OK) err=rd_i8(h,KEY_A_BCLK,DEFAULT_I2S_BCLK_PIN,&a->i2s_bclk_pin);
    if(err==ESP_OK) err=rd_i8(h,KEY_A_WS,DEFAULT_I2S_WS_PIN,&a->i2s_ws_pin);
    if(err==ESP_OK) err=rd_i8(h,KEY_A_DATA,DEFAULT_I2S_DATA_PIN,&a->i2s_data_pin);
    RU32(KEY_A_RATE,DEFAULT_AUDIO_SAMPLE_RATE,a->sample_rate);
    RU16(KEY_A_BUF,DEFAULT_AUDIO_BUFFER_SIZE,a->buffer_size);
    RU16(KEY_A_FFT_SZ,DEFAULT_AUDIO_FFT_SIZE,a->fft_size);
    RU8D(KEY_A_SENS,DEFAULT_AUDIO_SENSITIVITY,a->sensitivity);
    RU8D(KEY_A_GAIN,DEFAULT_AUDIO_GAIN,a->gain);
    RBOOL(KEY_A_AGC,DEFAULT_AUDIO_AUTO_GAIN,a->auto_gain);
    RU16(KEY_A_NGATE,DEFAULT_AUDIO_NOISE_GATE,a->noise_gate);
    RU8D(KEY_A_SMTH,DEFAULT_AUDIO_SMOOTHING,a->smoothing);
    RU16(KEY_A_BEAT_TH,DEFAULT_AUDIO_BEAT_THRESHOLD,a->beat_threshold);
    RBOOL(KEY_A_FFT_EN,DEFAULT_AUDIO_FFT_ENABLED,a->fft_enabled);
    RU8D(KEY_A_BANDS,DEFAULT_AUDIO_SPECTRUM_BANDS,a->spectrum_bands);
    RBOOL(KEY_A_BBOOST,0,a->bass_boost);
    RBOOL(KEY_A_TBOOST,0,a->treble_boost);

    /* matrix */
    matrix_config_t *m = &out->matrix;
    RBOOL(KEY_M_EN,DEFAULT_MATRIX_ENABLED,m->enabled);
    RU16(KEY_M_W,DEFAULT_MATRIX_WIDTH,m->width);
    RU16(KEY_M_H,DEFAULT_MATRIX_HEIGHT,m->height);
    RU8(KEY_M_LAYOUT,(uint8_t)MATRIX_LAYOUT_SERPENTINE,m->layout);
    RU8(KEY_M_ORIGIN,(uint8_t)MATRIX_ORIGIN_TOP_LEFT,m->origin);
    RBOOL(KEY_M_RX,0,m->reverse_x);
    RBOOL(KEY_M_RY,0,m->reverse_y);
    RBOOL(KEY_M_ROT90,0,m->rotate_90);

    /* random reactive */
    RBOOL(KEY_RR_EN,0,out->random_reactive.enabled);
    RU16(KEY_RR_INTV,DEFAULT_RANDOM_INTERVAL,out->random_reactive.interval_seconds);
    RBOOL(KEY_RR_NOREP,1,out->random_reactive.no_repeat);
    RBOOL(KEY_RR_INC_S,1,out->random_reactive.include_strip);
    RBOOL(KEY_RR_INC_M,1,out->random_reactive.include_matrix);
    RBOOL(KEY_RR_FAVS,0,out->random_reactive.only_favorites);

    /* random normal */
    RBOOL(KEY_RN_EN,0,out->random_normal.enabled);
    RU16(KEY_RN_INTV,DEFAULT_RANDOM_INTERVAL,out->random_normal.interval_seconds);
    RBOOL(KEY_RN_NOREP,1,out->random_normal.no_repeat);
    RBOOL(KEY_RN_FAVS,0,out->random_normal.only_favorites);

    /* display */
    RBOOL(KEY_D_EN,DEFAULT_DISPLAY_ENABLED,out->display.enabled);
    RU8D(KEY_D_BRIGHT,DEFAULT_DISPLAY_BRIGHTNESS,out->display.brightness);
    RU8(KEY_D_THEME,(uint8_t)DISPLAY_THEME_NEON_DARK,out->display.theme);
    RU8(KEY_D_VIEW,(uint8_t)DISPLAY_VIEW_AUTO,out->display.view_mode);
    RBOOL(KEY_D_FPS,1,out->display.show_fps);
    RBOOL(KEY_D_WIFI,1,out->display.show_wifi);

    #undef RU8
    #undef RU8D
    #undef RBOOL
    #undef RU16
    #undef RU32

    nvs_close(h);
    if (err!=ESP_OK) { config_manager_default(out); return ESP_OK; }
    if (config_manager_validate(out)!=ESP_OK) { config_manager_default(out); return ESP_OK; }
    ESP_LOGI(TAG,"loaded mode=%s",config_manager_operating_mode_to_string(out->operating_mode));
    return ESP_OK;
}

esp_err_t config_manager_save(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    esp_err_t err = config_manager_validate(cfg);
    if (err!=ESP_OK) return err;

    nvs_handle_t h;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err!=ESP_OK) return err;

    #define WU8(k,v) if(err==ESP_OK) err=nvs_set_u8(h,k,(uint8_t)(v))
    #define WU16(k,v) if(err==ESP_OK) err=nvs_set_u16(h,k,(uint16_t)(v))
    #define WU32(k,v) if(err==ESP_OK) err=nvs_set_u32(h,k,(uint32_t)(v))
    #define WI8(k,v) if(err==ESP_OK) err=nvs_set_i8(h,k,(int8_t)(v))

    err=nvs_set_str(h,KEY_DEVICE_NAME,cfg->device_name);

    WU8(KEY_OP_MODE, cfg->operating_mode);
    WU8(KEY_LED_TYPE,cfg->led_type); WU8(KEY_LED_PIN,cfg->led_pin);
    WU16(KEY_LED_COUNT,cfg->led_count); WU8(KEY_BRIGHTNESS,cfg->brightness);
    WU8(KEY_POWER,cfg->power?1:0);
    WU8(KEY_COLOR_R,cfg->color_r); WU8(KEY_COLOR_G,cfg->color_g);
    WU8(KEY_COLOR_B,cfg->color_b); WU8(KEY_COLOR_W,cfg->color_w);
    WU8(KEY_SEC_R,cfg->sec_r); WU8(KEY_SEC_G,cfg->sec_g);
    WU8(KEY_SEC_B,cfg->sec_b); WU8(KEY_SEC_W,cfg->sec_w);
    WU8(KEY_BG_R,cfg->bg_r); WU8(KEY_BG_G,cfg->bg_g);
    WU8(KEY_BG_B,cfg->bg_b); WU8(KEY_BG_W,cfg->bg_w);
    WU8(KEY_ANIMATION,cfg->animation); WU8(KEY_ANIM_SPEED,cfg->animation_speed);
    WU8(KEY_ANIM_DIR,cfg->animation_direction);
    WU8(KEY_ANIM_SIZE,cfg->animation_size); WU8(KEY_TAIL_LEN,cfg->tail_length);
    WU8(KEY_FADE_AMT,cfg->fade_amount); WU8(KEY_DENSITY,cfg->density);
    WU8(KEY_INTENSITY,cfg->intensity); WU8(KEY_COOLING,cfg->cooling);
    WU8(KEY_SPARKING,cfg->sparking);
    WU8(KEY_ANIM_LOOP,cfg->anim_loop?1:0); WU8(KEY_MIRROR,cfg->mirror?1:0);
    WU8(KEY_RAND_COLOR,cfg->random_color?1:0); WU8(KEY_CUST_PAT,cfg->custom_pattern);

    WU8(KEY_RX_EFFECT, cfg->active_reactive_effect);
    WU8(KEY_MX_EFFECT, cfg->active_matrix_effect);
    WU8(KEY_RX_MX_EFFE,cfg->active_rx_matrix_effect);
    WU8(KEY_PALETTE, cfg->palette_id);

    /* audio */
    const audio_config_t *a = &cfg->audio;
    WI8(KEY_A_BCLK,a->i2s_bclk_pin); WI8(KEY_A_WS,a->i2s_ws_pin); WI8(KEY_A_DATA,a->i2s_data_pin);
    WU32(KEY_A_RATE,a->sample_rate); WU16(KEY_A_BUF,a->buffer_size); WU16(KEY_A_FFT_SZ,a->fft_size);
    WU8(KEY_A_SENS,a->sensitivity); WU8(KEY_A_GAIN,a->gain); WU8(KEY_A_AGC,a->auto_gain?1:0);
    WU16(KEY_A_NGATE,a->noise_gate); WU8(KEY_A_SMTH,a->smoothing); WU16(KEY_A_BEAT_TH,a->beat_threshold);
    WU8(KEY_A_FFT_EN,a->fft_enabled?1:0); WU8(KEY_A_BANDS,a->spectrum_bands);
    WU8(KEY_A_BBOOST,a->bass_boost?1:0); WU8(KEY_A_TBOOST,a->treble_boost?1:0);

    /* matrix */
    const matrix_config_t *m = &cfg->matrix;
    WU8(KEY_M_EN,m->enabled?1:0); WU16(KEY_M_W,m->width); WU16(KEY_M_H,m->height);
    WU8(KEY_M_LAYOUT,m->layout); WU8(KEY_M_ORIGIN,m->origin);
    WU8(KEY_M_RX,m->reverse_x?1:0); WU8(KEY_M_RY,m->reverse_y?1:0); WU8(KEY_M_ROT90,m->rotate_90?1:0);

    /* random reactive */
    WU8(KEY_RR_EN,cfg->random_reactive.enabled?1:0);
    WU16(KEY_RR_INTV,cfg->random_reactive.interval_seconds);
    WU8(KEY_RR_NOREP,cfg->random_reactive.no_repeat?1:0);
    WU8(KEY_RR_INC_S,cfg->random_reactive.include_strip?1:0);
    WU8(KEY_RR_INC_M,cfg->random_reactive.include_matrix?1:0);
    WU8(KEY_RR_FAVS,cfg->random_reactive.only_favorites?1:0);

    WU8(KEY_RN_EN,cfg->random_normal.enabled?1:0);
    WU16(KEY_RN_INTV,cfg->random_normal.interval_seconds);
    WU8(KEY_RN_NOREP,cfg->random_normal.no_repeat?1:0);
    WU8(KEY_RN_FAVS,cfg->random_normal.only_favorites?1:0);

    WU8(KEY_D_EN,cfg->display.enabled?1:0);
    WU8(KEY_D_BRIGHT,cfg->display.brightness);
    WU8(KEY_D_THEME,cfg->display.theme);
    WU8(KEY_D_VIEW,cfg->display.view_mode);
    WU8(KEY_D_FPS,cfg->display.show_fps?1:0);
    WU8(KEY_D_WIFI,cfg->display.show_wifi?1:0);

    #undef WU8
    #undef WU16
    #undef WU32
    #undef WI8

    if (err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t config_manager_factory_reset(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err!=ESP_OK) return err;
    err=nvs_erase_all(h);
    if (err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err;
}
