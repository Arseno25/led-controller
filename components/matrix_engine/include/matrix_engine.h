#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"
#include "led_driver.h"
#include "audio_processor.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t matrix_engine_init(const matrix_config_t *cfg);
esp_err_t matrix_engine_set_config(const matrix_config_t *cfg);
esp_err_t matrix_engine_set_params(const app_config_t *app);

uint16_t  matrix_engine_xy_to_index(uint16_t x, uint16_t y);
uint16_t  matrix_engine_get_width(void);
uint16_t  matrix_engine_get_height(void);

/* Render normal matrix effect */
esp_err_t matrix_engine_render_normal(matrix_effect_t effect, uint32_t delta_ms);

/* Render reactive matrix effect */
esp_err_t matrix_engine_render_reactive(rx_matrix_effect_t effect,
                                        const audio_features_t *feat,
                                        uint32_t delta_ms);

esp_err_t matrix_engine_reset(void);

#ifdef __cplusplus
}
#endif
