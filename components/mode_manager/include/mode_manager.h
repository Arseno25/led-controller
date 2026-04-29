#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t        mode_manager_init(operating_mode_t initial_mode);
esp_err_t        mode_manager_set_mode(operating_mode_t mode);
operating_mode_t mode_manager_get_mode(void);
esp_err_t        mode_manager_set_matrix_effect(matrix_effect_t effect);
matrix_effect_t  mode_manager_get_matrix_effect(void);

bool mode_manager_is_normal(void);
bool mode_manager_is_reactive(void);
bool mode_manager_is_matrix(void);
bool mode_manager_is_reactive_matrix(void);

esp_err_t mode_manager_update(uint32_t delta_ms);

#ifdef __cplusplus
}
#endif
