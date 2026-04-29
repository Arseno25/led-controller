#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t reactive_matrix_renderer_init(void);
esp_err_t reactive_matrix_renderer_set_effect(rx_matrix_effect_t effect);
rx_matrix_effect_t reactive_matrix_renderer_get_effect(void);
esp_err_t reactive_matrix_renderer_set_params(const app_config_t *cfg);
esp_err_t reactive_matrix_renderer_update(uint32_t delta_ms);
esp_err_t reactive_matrix_renderer_reset(void);

#ifdef __cplusplus
}
#endif
