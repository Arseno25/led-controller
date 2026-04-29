#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "config_manager.h"
#include "led_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    animation_type_t      type;
    led_color_t           primary_color;
    led_color_t           secondary_color;
    led_color_t           background_color;
    uint8_t               speed;
    uint8_t               brightness;
    animation_direction_t direction;
    uint8_t               size;
    uint8_t               tail_length;
    uint8_t               fade_amount;
    uint8_t               density;
    uint8_t               intensity;
    uint8_t               cooling;
    uint8_t               sparking;
    bool                  loop;
    bool                  mirror;
    bool                  random_color;
    custom_pattern_type_t custom_pattern;
    bool                  power;
} animation_config_t;

/* Renderer function pointer */
typedef void (*animation_render_fn_t)(const animation_config_t *cfg, uint16_t count);

typedef struct {
    animation_type_t      type;
    const char           *name;
    animation_render_fn_t render;
} animation_renderer_t;

esp_err_t animation_layer_init(void);
esp_err_t animation_layer_start(void);
esp_err_t animation_layer_stop(void);

esp_err_t animation_layer_set_config(const animation_config_t *config);
esp_err_t animation_layer_get_config(animation_config_t *config);

esp_err_t animation_layer_set_animation(animation_type_t type);
esp_err_t animation_layer_set_speed(uint8_t speed);
esp_err_t animation_layer_set_brightness(uint8_t brightness);
esp_err_t animation_layer_set_power(bool power);

esp_err_t animation_layer_pause(void);
esp_err_t animation_layer_resume(void);

/* Called by mode_manager to render one frame without internal task */
esp_err_t animation_layer_update(uint32_t delta_ms);

#ifdef __cplusplus
}
#endif
