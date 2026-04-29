#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "board_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_DEVICE_NAME_MAX_LEN 32

#define CONFIG_LED_COUNT_MIN 1
#define CONFIG_LED_COUNT_MAX 960

#define MATRIX_MAX_W 32
#define MATRIX_MAX_H 32

#define AUDIO_SPECTRUM_BANDS_MAX 16

/* ---- LED defaults ---- */
#define DEFAULT_DEVICE_NAME      "Pixel Controller"
#define DEFAULT_LED_TYPE         LED_TYPE_WS2812B
#define DEFAULT_LED_PIN          BOARD_DEFAULT_LED_PIN
#define DEFAULT_LED_COUNT        60
#define DEFAULT_BRIGHTNESS       128
#define DEFAULT_POWER            true
#define DEFAULT_COLOR_R          255
#define DEFAULT_COLOR_G          100
#define DEFAULT_COLOR_B          0
#define DEFAULT_COLOR_W          0

/* ---- Animation defaults ---- */
#define DEFAULT_ANIMATION        ANIM_SOLID
#define DEFAULT_ANIMATION_SPEED  80
#define DEFAULT_ANIM_DIRECTION   ANIM_DIR_FORWARD
#define DEFAULT_ANIM_SIZE        3
#define DEFAULT_TAIL_LENGTH      12
#define DEFAULT_FADE_AMOUNT      40
#define DEFAULT_DENSITY          30
#define DEFAULT_INTENSITY        80
#define DEFAULT_COOLING          55
#define DEFAULT_SPARKING         120
#define DEFAULT_CUSTOM_PATTERN   CUSTOM_PATTERN_DOT

/* ---- I2S audio (INMP441) defaults ---- */
#define DEFAULT_I2S_BCLK_PIN          BOARD_DEFAULT_I2S_BCLK_PIN
#define DEFAULT_I2S_WS_PIN            BOARD_DEFAULT_I2S_WS_PIN
#define DEFAULT_I2S_DATA_PIN          BOARD_DEFAULT_I2S_DATA_PIN
#define DEFAULT_AUDIO_SAMPLE_RATE     16000
#define DEFAULT_AUDIO_BUFFER_SIZE     512
#define DEFAULT_AUDIO_FFT_SIZE        512
#define DEFAULT_AUDIO_SENSITIVITY     110
#define DEFAULT_AUDIO_GAIN            140
#define DEFAULT_AUDIO_AUTO_GAIN       true
#define DEFAULT_AUDIO_NOISE_GATE      35
#define DEFAULT_AUDIO_SMOOTHING       60
#define DEFAULT_AUDIO_BEAT_THRESHOLD  650
#define DEFAULT_AUDIO_FFT_ENABLED     true
#define DEFAULT_AUDIO_SPECTRUM_BANDS  16

/* ---- Matrix defaults ---- */
#define DEFAULT_MATRIX_ENABLED   false
#define DEFAULT_MATRIX_WIDTH     16
#define DEFAULT_MATRIX_HEIGHT    16

/* ---- Random effect defaults ---- */
#define DEFAULT_RANDOM_INTERVAL  60

/* ---- GC9A01 display defaults ---- */
#define DEFAULT_DISPLAY_ENABLED    true
#define DEFAULT_DISPLAY_BRIGHTNESS 180
#define DISPLAY_THEME_MAX_LEN      16

/* ========== ENUMS ========== */

typedef enum {
    OPERATING_MODE_NORMAL = 0,
    OPERATING_MODE_REACTIVE,
    OPERATING_MODE_MATRIX,
    OPERATING_MODE_REACTIVE_MATRIX,
    OPERATING_MODE_MAX
} operating_mode_t;

typedef enum {
    LED_TYPE_WS2812B = 0,
    LED_TYPE_WS2811,
    LED_TYPE_WS2813,
    LED_TYPE_WS2815,
    LED_TYPE_SK6812_RGB,
    LED_TYPE_SK6812_RGBW,
    LED_TYPE_APA102,
    LED_TYPE_SK9822,
    LED_TYPE_NEON_PIXEL_5V,
    LED_TYPE_NEON_PIXEL_12V,
    LED_TYPE_MAX
} led_type_t;

typedef enum {
    ANIM_SOLID = 0,
    ANIM_BLINK,
    ANIM_BREATHING,
    ANIM_RAINBOW,
    ANIM_COLOR_WIPE,
    ANIM_RUNNING_DOT,
    ANIM_COMET,
    ANIM_METEOR_RAIN,
    ANIM_THEATER_CHASE,
    ANIM_SCANNER,
    ANIM_TWINKLE,
    ANIM_SPARKLE,
    ANIM_FIRE,
    ANIM_POLICE_LIGHT,
    ANIM_NEON_FLICKER,
    ANIM_GRADIENT_FLOW,
    ANIM_WAVE,
    ANIM_CONFETTI,
    ANIM_PULSE,
    ANIM_CUSTOM,
    ANIM_TYPE_MAX
} animation_type_t;

typedef enum {
    ANIM_DIR_FORWARD = 0,
    ANIM_DIR_REVERSE,
    ANIM_DIR_BOUNCE,
    ANIM_DIR_MAX
} animation_direction_t;

typedef enum {
    CUSTOM_PATTERN_DOT = 0,
    CUSTOM_PATTERN_BAR,
    CUSTOM_PATTERN_GRADIENT,
    CUSTOM_PATTERN_WAVE,
    CUSTOM_PATTERN_RANDOM_SPARK,
    CUSTOM_PATTERN_DUAL_COLOR_CHASE,
    CUSTOM_PATTERN_MAX
} custom_pattern_type_t;

/* Reactive strip effects (INMP441-driven) */
typedef enum {
    REACTIVE_EFFECT_VU_BAR = 0,
    REACTIVE_EFFECT_PULSE,
    REACTIVE_EFFECT_BEAT_FLASH,
    REACTIVE_EFFECT_SPARK,
    REACTIVE_EFFECT_COMET,
    REACTIVE_EFFECT_CHASE,
    REACTIVE_EFFECT_SPECTRUM_BARS,
    REACTIVE_EFFECT_BASS_HIT,
    REACTIVE_EFFECT_MAX
} reactive_effect_t;

/* Matrix normal effects */
typedef enum {
    MATRIX_EFFECT_RAIN = 0,
    MATRIX_EFFECT_RAINBOW,
    MATRIX_EFFECT_FIRE,
    MATRIX_EFFECT_PLASMA,
    MATRIX_EFFECT_LAVA,
    MATRIX_EFFECT_AURORA,
    MATRIX_EFFECT_RIPPLE,
    MATRIX_EFFECT_STARFIELD,
    MATRIX_EFFECT_WATERFALL,
    MATRIX_EFFECT_BLOCKS,
    MATRIX_EFFECT_NOISE_FLOW,
    MATRIX_EFFECT_SPIRAL,
    MATRIX_EFFECT_MAX
} matrix_effect_t;

/* Reactive matrix effects */
typedef enum {
    RX_MATRIX_SPECTRUM_BARS = 0,
    RX_MATRIX_CENTER_VU,
    RX_MATRIX_BASS_PULSE,
    RX_MATRIX_AUDIO_RIPPLE,
    RX_MATRIX_BEAT_FLASH,
    RX_MATRIX_SPARK_FIELD,
    RX_MATRIX_AUDIO_PLASMA,
    RX_MATRIX_FIRE_EQ,
    RX_MATRIX_BASS_TUNNEL,
    RX_MATRIX_TREBLE_RAIN,
    RX_MATRIX_MID_WAVE,
    RX_MATRIX_AUDIO_BLOCKS,
    RX_MATRIX_REACTIVE_AURORA,
    RX_MATRIX_REACTIVE_SPIRAL,
    RX_MATRIX_SPECTRUM_RAINBOW,
    RX_MATRIX_SPECTRUM_MIRROR,
    RX_MATRIX_SPECTRUM_PEAKS,
    RX_MATRIX_SPECTRUM_WATERFALL,
    RX_MATRIX_SEGMENT_VU,
    RX_MATRIX_MAX
} rx_matrix_effect_t;

typedef enum {
    MATRIX_LAYOUT_SERPENTINE = 0,
    MATRIX_LAYOUT_PROGRESSIVE,
    MATRIX_LAYOUT_MAX
} matrix_layout_t;

typedef enum {
    MATRIX_ORIGIN_TOP_LEFT = 0,
    MATRIX_ORIGIN_TOP_RIGHT,
    MATRIX_ORIGIN_BOTTOM_LEFT,
    MATRIX_ORIGIN_BOTTOM_RIGHT,
    MATRIX_ORIGIN_MAX
} matrix_origin_t;

typedef enum {
    DISPLAY_THEME_NEON_DARK = 0,
    DISPLAY_THEME_MAX
} display_theme_t;

typedef enum {
    DISPLAY_VIEW_AUTO = 0,
    DISPLAY_VIEW_STATUS,
    DISPLAY_VIEW_SPECTRUM,
    DISPLAY_VIEW_VU_METER,
    DISPLAY_VIEW_WAVEFORM,
    DISPLAY_VIEW_MAX
} display_view_mode_t;

/* ========== STRUCTS ========== */

typedef struct {
    /* I2S pins */
    int8_t i2s_bclk_pin;
    int8_t i2s_ws_pin;
    int8_t i2s_data_pin;

    uint32_t sample_rate;
    uint16_t buffer_size;
    uint16_t fft_size;

    uint8_t  sensitivity;
    uint8_t  gain;
    bool     auto_gain;

    uint16_t noise_gate;
    uint8_t  smoothing;
    uint16_t beat_threshold;

    bool     fft_enabled;
    uint8_t  spectrum_bands;

    bool     bass_boost;
    bool     treble_boost;
} audio_config_t;

typedef struct {
    bool             enabled;
    uint16_t         width;
    uint16_t         height;
    matrix_layout_t  layout;
    matrix_origin_t  origin;
    bool             reverse_x;
    bool             reverse_y;
    bool             rotate_90;
} matrix_config_t;

typedef struct {
    bool     enabled;
    uint16_t interval_seconds;
    bool     no_repeat;
    bool     include_strip;
    bool     include_matrix;
    bool     only_favorites;
} random_reactive_config_t;

typedef struct {
    bool     enabled;
    uint16_t interval_seconds;
    bool     no_repeat;
    bool     only_favorites;
} random_normal_config_t;

typedef struct {
    bool                enabled;
    uint8_t             brightness;
    display_theme_t     theme;
    display_view_mode_t view_mode;
    bool                show_fps;
    bool                show_wifi;
} display_config_t;

typedef struct {
    char device_name[CONFIG_DEVICE_NAME_MAX_LEN];

    operating_mode_t operating_mode;

    /* LED config */
    led_type_t led_type;
    uint8_t led_pin;
    uint16_t led_count;
    uint8_t brightness;
    bool power;

    /* primary color */
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    uint8_t color_w;

    /* secondary color */
    uint8_t sec_r;
    uint8_t sec_g;
    uint8_t sec_b;
    uint8_t sec_w;

    /* background color */
    uint8_t bg_r;
    uint8_t bg_g;
    uint8_t bg_b;
    uint8_t bg_w;

    /* normal animation */
    animation_type_t animation;
    uint8_t animation_speed;
    animation_direction_t animation_direction;

    /* normal animation params */
    uint8_t animation_size;
    uint8_t tail_length;
    uint8_t fade_amount;
    uint8_t density;
    uint8_t intensity;
    uint8_t cooling;
    uint8_t sparking;

    /* animation flags */
    bool anim_loop;
    bool mirror;
    bool random_color;

    /* custom animation */
    custom_pattern_type_t custom_pattern;

    /* mode-specific active effects */
    reactive_effect_t  active_reactive_effect;
    matrix_effect_t    active_matrix_effect;
    rx_matrix_effect_t active_rx_matrix_effect;

    /* palette */
    uint8_t palette_id;

    /* sub-configs */
    audio_config_t           audio;
    matrix_config_t          matrix;
    random_reactive_config_t random_reactive;
    random_normal_config_t   random_normal;
    display_config_t         display;
} app_config_t;

/* ========== API ========== */

esp_err_t config_manager_init(void);
esp_err_t config_manager_load(app_config_t *out);
esp_err_t config_manager_save(const app_config_t *cfg);
esp_err_t config_manager_validate(const app_config_t *cfg);
esp_err_t config_manager_factory_reset(void);
void      config_manager_default(app_config_t *out);
bool      config_manager_led_pin_is_safe(uint8_t pin);
bool      config_manager_led_type_is_valid(led_type_t type);

const char *config_manager_led_type_to_string(led_type_t type);
esp_err_t   config_manager_led_type_from_string(const char *str, led_type_t *out);

bool        config_manager_animation_is_valid(animation_type_t a);
const char *config_manager_animation_to_string(animation_type_t a);
esp_err_t   config_manager_animation_from_string(const char *str, animation_type_t *out);

bool        config_manager_direction_is_valid(animation_direction_t d);
const char *config_manager_direction_to_string(animation_direction_t d);
esp_err_t   config_manager_direction_from_string(const char *str, animation_direction_t *out);

bool        config_manager_custom_pattern_is_valid(custom_pattern_type_t p);
const char *config_manager_custom_pattern_to_string(custom_pattern_type_t p);
esp_err_t   config_manager_custom_pattern_from_string(const char *str, custom_pattern_type_t *out);

const char *config_manager_operating_mode_to_string(operating_mode_t m);
esp_err_t   config_manager_operating_mode_from_string(const char *str, operating_mode_t *out);

const char *config_manager_reactive_effect_to_string(reactive_effect_t e);
esp_err_t   config_manager_reactive_effect_from_string(const char *str, reactive_effect_t *out);

const char *config_manager_matrix_effect_to_string(matrix_effect_t e);
esp_err_t   config_manager_matrix_effect_from_string(const char *str, matrix_effect_t *out);

const char *config_manager_rx_matrix_effect_to_string(rx_matrix_effect_t e);
esp_err_t   config_manager_rx_matrix_effect_from_string(const char *str, rx_matrix_effect_t *out);

const char *config_manager_matrix_layout_to_string(matrix_layout_t l);
esp_err_t   config_manager_matrix_layout_from_string(const char *str, matrix_layout_t *out);

const char *config_manager_matrix_origin_to_string(matrix_origin_t o);
esp_err_t   config_manager_matrix_origin_from_string(const char *str, matrix_origin_t *out);

const char *config_manager_display_theme_to_string(display_theme_t t);
esp_err_t   config_manager_display_theme_from_string(const char *str, display_theme_t *out);

const char *config_manager_display_view_to_string(display_view_mode_t v);
esp_err_t   config_manager_display_view_from_string(const char *str, display_view_mode_t *out);

#ifdef __cplusplus
}
#endif
