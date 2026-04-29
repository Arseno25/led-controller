#pragma once

#include "esp_err.h"
#include "config_manager.h"
#include "led_driver.h"
#include "audio_processor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_err_t (*on_config_update)(const app_config_t *new_cfg);
    esp_err_t (*on_color_update)(const led_color_t *color);
    esp_err_t (*on_brightness_update)(uint8_t brightness);
    esp_err_t (*on_power_update)(bool power);
    esp_err_t (*on_animation_update)(const app_config_t *anim_cfg);
    esp_err_t (*on_factory_reset)(void);
    const app_config_t *(*get_config)(void);

    /* audio (INMP441) callbacks */
    esp_err_t (*on_audio_config_update)(const audio_config_t *audio_cfg);
    esp_err_t (*get_audio_features)(audio_features_t *out);

    /* mode callbacks */
    esp_err_t (*on_mode_update)(operating_mode_t mode);
    esp_err_t (*on_reactive_effect_update)(reactive_effect_t effect);
    esp_err_t (*on_matrix_effect_update)(matrix_effect_t effect);
    esp_err_t (*on_rx_matrix_effect_update)(rx_matrix_effect_t effect);

    /* matrix + random + palette */
    esp_err_t (*on_matrix_config_update)(const matrix_config_t *cfg);
    esp_err_t (*on_random_reactive_update)(const random_reactive_config_t *cfg);
    esp_err_t (*on_random_normal_update)(const random_normal_config_t *cfg);
    esp_err_t (*on_random_next)(operating_mode_t mode);
    esp_err_t (*on_palette_update)(uint8_t palette_id);

    /* GC9A01 display callbacks */
    esp_err_t (*on_display_config_update)(const display_config_t *cfg);
    esp_err_t (*on_display_view_update)(display_view_mode_t view);
} web_server_callbacks_t;

esp_err_t web_server_start(const web_server_callbacks_t *cb);
esp_err_t web_server_stop(void);

#ifdef __cplusplus
}
#endif
