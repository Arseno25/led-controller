#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t random_effect_manager_init(void);
esp_err_t random_effect_manager_set_random_reactive(const random_reactive_config_t *cfg);
esp_err_t random_effect_manager_set_random_normal(const random_normal_config_t *cfg);

/* Tick from main loop. Returns true if a new effect was selected. */
bool random_effect_manager_tick_reactive(operating_mode_t mode,
                                         reactive_effect_t *strip_inout,
                                         rx_matrix_effect_t *matrix_inout,
                                         uint32_t delta_ms);

bool random_effect_manager_tick_normal(animation_type_t *anim_inout, uint32_t delta_ms);

esp_err_t random_effect_manager_force_next(operating_mode_t mode);

/* Tracks whether next tick should fire immediately */
void random_effect_manager_request_next(void);

#ifdef __cplusplus
}
#endif
