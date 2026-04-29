#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    led_type_t type;
    uint8_t pin;
    uint16_t count;
    uint8_t brightness;
    bool rgbw;
} led_driver_config_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
} led_color_t;

esp_err_t led_driver_init(const led_driver_config_t *config);
esp_err_t led_driver_deinit(void);
esp_err_t led_driver_reinit(const led_driver_config_t *config);

esp_err_t led_driver_set_all(led_color_t color);
esp_err_t led_driver_set_pixel(uint16_t index, led_color_t color);
esp_err_t led_driver_set_brightness(uint8_t brightness);
esp_err_t led_driver_clear(void);
esp_err_t led_driver_show(void);

uint16_t  led_driver_get_count(void);
bool      led_driver_type_supports_white(led_type_t type);

#ifdef __cplusplus
}
#endif
