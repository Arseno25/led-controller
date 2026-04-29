#include "led_driver.h"

#include <string.h>

#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "led_driver";

#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static led_strip_handle_t s_strip          = NULL;
static led_driver_config_t s_cfg           = {0};
static bool s_initialized                  = false;

bool led_driver_type_supports_white(led_type_t type)
{
    return type == LED_TYPE_SK6812_RGBW;
}

static esp_err_t map_type_to_model(led_type_t type, led_model_t *out_model)
{
    switch (type) {
        case LED_TYPE_WS2812B:
        case LED_TYPE_WS2811:
        case LED_TYPE_WS2813:
        case LED_TYPE_WS2815:
        case LED_TYPE_NEON_PIXEL_5V:
        case LED_TYPE_NEON_PIXEL_12V:
            *out_model = LED_MODEL_WS2812;
            return ESP_OK;
        case LED_TYPE_SK6812_RGB:
        case LED_TYPE_SK6812_RGBW:
            *out_model = LED_MODEL_SK6812;
            return ESP_OK;
        case LED_TYPE_APA102:
            ESP_LOGW(TAG, "APA102 not supported in MVP");
            return ESP_ERR_NOT_SUPPORTED;
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t led_driver_init(const led_driver_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        ESP_LOGW(TAG, "already initialized; call reinit instead");
        return ESP_ERR_INVALID_STATE;
    }

    led_model_t model;
    esp_err_t err = map_type_to_model(config->type, &model);
    if (err != ESP_OK) {
        return err;
    }

    led_strip_config_t strip_cfg = {
        .strip_gpio_num   = config->pin,
        .max_leds         = config->count,
        .led_pixel_format = config->rgbw ? LED_PIXEL_FORMAT_GRBW : LED_PIXEL_FORMAT_GRB,
        .led_model        = model,
        .flags            = {
            .invert_out = 0,
        },
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = LED_STRIP_RMT_RES_HZ,
        .mem_block_symbols = 64,
        .flags             = {
            .with_dma = 0,
        },
    };

    err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        s_strip = NULL;
        return err;
    }

    s_cfg          = *config;
    s_initialized  = true;

    ESP_LOGI(TAG, "init OK type=%d pin=%u count=%u rgbw=%d brightness=%u",
             config->type, config->pin, config->count, config->rgbw, config->brightness);

    led_strip_clear(s_strip);
    return ESP_OK;
}

esp_err_t led_driver_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    led_strip_clear(s_strip);
    esp_err_t err = led_strip_del(s_strip);
    s_strip       = NULL;
    s_initialized = false;
    memset(&s_cfg, 0, sizeof(s_cfg));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_del failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t led_driver_reinit(const led_driver_config_t *config)
{
    esp_err_t err = led_driver_deinit();
    if (err != ESP_OK) {
        return err;
    }
    return led_driver_init(config);
}

static uint8_t scale8(uint8_t v, uint8_t brightness)
{
    return (uint8_t)(((uint16_t)v * (uint16_t)brightness) / 255);
}

esp_err_t led_driver_set_pixel(uint16_t index, led_color_t color)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (index >= s_cfg.count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t r = scale8(color.r, s_cfg.brightness);
    uint8_t g = scale8(color.g, s_cfg.brightness);
    uint8_t b = scale8(color.b, s_cfg.brightness);

    if (s_cfg.rgbw) {
        uint8_t w = scale8(color.w, s_cfg.brightness);
        return led_strip_set_pixel_rgbw(s_strip, index, r, g, b, w);
    }
    return led_strip_set_pixel(s_strip, index, r, g, b);
}

esp_err_t led_driver_set_all(led_color_t color)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    for (uint16_t i = 0; i < s_cfg.count; ++i) {
        esp_err_t err = led_driver_set_pixel(i, color);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t led_driver_set_brightness(uint8_t brightness)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    s_cfg.brightness = brightness;
    return ESP_OK;
}

esp_err_t led_driver_clear(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_clear(s_strip);
}

esp_err_t led_driver_show(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_refresh(s_strip);
}

uint16_t led_driver_get_count(void)
{
    return s_initialized ? s_cfg.count : 0;
}
