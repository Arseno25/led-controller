#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "led_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PALETTE_MAX_COLORS 8

typedef struct {
    uint8_t        id;
    const char    *name;
    led_color_t    colors[PALETTE_MAX_COLORS];
    uint8_t        color_count;
} palette_t;

esp_err_t       palette_manager_init(void);
size_t          palette_manager_count(void);
const palette_t *palette_manager_get(uint8_t id);
const palette_t *palette_manager_get_by_name(const char *name);

/* Sample palette at t=0..255, smooth interpolation between stops */
led_color_t     palette_manager_sample(uint8_t id, uint8_t t);

#ifdef __cplusplus
}
#endif
