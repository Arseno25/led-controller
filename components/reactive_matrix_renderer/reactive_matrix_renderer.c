#include "reactive_matrix_renderer.h"
#include "matrix_engine.h"
#include "audio_processor.h"
#include "led_driver.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "rx_mtx_render";

static rx_matrix_effect_t s_effect = RX_MATRIX_SPECTRUM_BARS;
static app_config_t s_cfg;
static bool s_cfg_ready = false;

esp_err_t reactive_matrix_renderer_init(void)
{
    s_effect = RX_MATRIX_SPECTRUM_BARS;
    ESP_LOGI(TAG, "init");
    return ESP_OK;
}

esp_err_t reactive_matrix_renderer_set_effect(rx_matrix_effect_t effect)
{
    if ((int)effect < 0 || (int)effect >= RX_MATRIX_MAX) return ESP_ERR_INVALID_ARG;
    if (effect != s_effect) {
        s_effect = effect;
        matrix_engine_reset();
        ESP_LOGI(TAG, "effect=%s", config_manager_rx_matrix_effect_to_string(effect));
    }
    return ESP_OK;
}

rx_matrix_effect_t reactive_matrix_renderer_get_effect(void) { return s_effect; }

esp_err_t reactive_matrix_renderer_set_params(const app_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_cfg = *cfg;
    s_cfg_ready = true;
    matrix_engine_set_params(cfg);
    return ESP_OK;
}

esp_err_t reactive_matrix_renderer_update(uint32_t delta_ms)
{
    if (s_cfg_ready && !s_cfg.power) {
        led_driver_clear();
        led_driver_show();
        return ESP_OK;
    }
    audio_features_t a;
    if (audio_processor_get_features(&a) != ESP_OK) memset(&a, 0, sizeof(a));
    esp_err_t err = matrix_engine_render_reactive(s_effect, &a, delta_ms);
    if (err == ESP_OK) led_driver_show();
    return err;
}

esp_err_t reactive_matrix_renderer_reset(void)
{
    return matrix_engine_reset();
}
