#include "mode_manager.h"
#include "animation_layer.h"
#include "reactive_renderer.h"
#include "reactive_matrix_renderer.h"
#include "matrix_engine.h"
#include "led_driver.h"
#include "esp_log.h"

static const char *TAG = "mode_mgr";

static operating_mode_t s_mode = OPERATING_MODE_NORMAL;
static matrix_effect_t  s_matrix_effect = MATRIX_EFFECT_RAIN;

esp_err_t mode_manager_init(operating_mode_t initial_mode)
{
    s_mode = initial_mode;
    ESP_LOGI(TAG, "init mode=%s", config_manager_operating_mode_to_string(initial_mode));
    return ESP_OK;
}

esp_err_t mode_manager_set_mode(operating_mode_t mode)
{
    if ((int)mode < 0 || (int)mode >= OPERATING_MODE_MAX) {
        ESP_LOGW(TAG, "invalid mode %d", (int)mode);
        return ESP_ERR_INVALID_ARG;
    }
    if (mode == s_mode) return ESP_OK;

    ESP_LOGI(TAG, "switch %s -> %s",
             config_manager_operating_mode_to_string(s_mode),
             config_manager_operating_mode_to_string(mode));

    /* Reset whichever renderer was active */
    switch (s_mode) {
        case OPERATING_MODE_REACTIVE:        reactive_renderer_reset(); break;
        case OPERATING_MODE_MATRIX:          matrix_engine_reset();      break;
        case OPERATING_MODE_REACTIVE_MATRIX: reactive_matrix_renderer_reset(); break;
        default: break;
    }

    led_driver_clear();
    led_driver_show();
    s_mode = mode;
    return ESP_OK;
}

operating_mode_t mode_manager_get_mode(void) { return s_mode; }

esp_err_t mode_manager_set_matrix_effect(matrix_effect_t effect)
{
    if ((int)effect < 0 || (int)effect >= MATRIX_EFFECT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (effect != s_matrix_effect) {
        s_matrix_effect = effect;
        matrix_engine_reset();
        ESP_LOGI(TAG, "matrix effect=%s", config_manager_matrix_effect_to_string(effect));
    }
    return ESP_OK;
}

matrix_effect_t mode_manager_get_matrix_effect(void) { return s_matrix_effect; }

bool mode_manager_is_normal(void)          { return s_mode == OPERATING_MODE_NORMAL; }
bool mode_manager_is_reactive(void)        { return s_mode == OPERATING_MODE_REACTIVE; }
bool mode_manager_is_matrix(void)          { return s_mode == OPERATING_MODE_MATRIX; }
bool mode_manager_is_reactive_matrix(void) { return s_mode == OPERATING_MODE_REACTIVE_MATRIX; }

esp_err_t mode_manager_update(uint32_t delta_ms)
{
    switch (s_mode) {
        case OPERATING_MODE_NORMAL:
            return animation_layer_update(delta_ms);
        case OPERATING_MODE_REACTIVE:
            return reactive_renderer_update(delta_ms);
        case OPERATING_MODE_MATRIX: {
            esp_err_t err = matrix_engine_render_normal(s_matrix_effect, delta_ms);
            if (err == ESP_OK) led_driver_show();
            return err;
        }
        case OPERATING_MODE_REACTIVE_MATRIX:
            return reactive_matrix_renderer_update(delta_ms);
        default:
            return ESP_ERR_INVALID_STATE;
    }
}
