#include "random_effect_manager.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "rand_fx";

static random_reactive_config_t s_rx;
static random_normal_config_t   s_norm;
static uint32_t s_rx_acc_ms = 0;
static uint32_t s_norm_acc_ms = 0;
static bool     s_force_next = false;

esp_err_t random_effect_manager_init(void)
{
    memset(&s_rx, 0, sizeof(s_rx));
    memset(&s_norm, 0, sizeof(s_norm));
    s_rx.interval_seconds = 60; s_rx.no_repeat = true;
    s_norm.interval_seconds = 60; s_norm.no_repeat = true;
    s_rx_acc_ms = s_norm_acc_ms = 0;
    return ESP_OK;
}

esp_err_t random_effect_manager_set_random_reactive(const random_reactive_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_rx = *cfg;
    s_rx_acc_ms = 0;
    return ESP_OK;
}

esp_err_t random_effect_manager_set_random_normal(const random_normal_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    s_norm = *cfg;
    s_norm_acc_ms = 0;
    return ESP_OK;
}

void random_effect_manager_request_next(void) { s_force_next = true; }

esp_err_t random_effect_manager_force_next(operating_mode_t mode)
{
    (void)mode;
    s_force_next = true;
    return ESP_OK;
}

bool random_effect_manager_tick_reactive(operating_mode_t mode,
                                         reactive_effect_t *strip_inout,
                                         rx_matrix_effect_t *matrix_inout,
                                         uint32_t delta_ms)
{
    if (!s_rx.enabled && !s_force_next) return false;
    if (mode != OPERATING_MODE_REACTIVE && mode != OPERATING_MODE_REACTIVE_MATRIX) return false;

    s_rx_acc_ms += delta_ms;
    uint32_t threshold_ms = (uint32_t)s_rx.interval_seconds * 1000U;
    if (threshold_ms < 5000) threshold_ms = 5000;

    if (!s_force_next && s_rx_acc_ms < threshold_ms) return false;
    s_rx_acc_ms = 0;
    s_force_next = false;

    bool changed = false;
    if (mode == OPERATING_MODE_REACTIVE && strip_inout && s_rx.include_strip) {
        reactive_effect_t cur = *strip_inout;
        reactive_effect_t pick = cur;
        for (int tries = 0; tries < 8; tries++) {
            pick = (reactive_effect_t)(esp_random() % REACTIVE_EFFECT_MAX);
            if (!s_rx.no_repeat || pick != cur) break;
        }
        if (pick != cur) {
            *strip_inout = pick;
            changed = true;
            ESP_LOGI(TAG, "rx strip -> %d", pick);
        }
    } else if (mode == OPERATING_MODE_REACTIVE_MATRIX && matrix_inout && s_rx.include_matrix) {
        rx_matrix_effect_t cur = *matrix_inout;
        rx_matrix_effect_t pick = cur;
        for (int tries = 0; tries < 8; tries++) {
            pick = (rx_matrix_effect_t)(esp_random() % RX_MATRIX_MAX);
            if (!s_rx.no_repeat || pick != cur) break;
        }
        if (pick != cur) {
            *matrix_inout = pick;
            changed = true;
            ESP_LOGI(TAG, "rx matrix -> %d", pick);
        }
    }
    return changed;
}

bool random_effect_manager_tick_normal(animation_type_t *anim_inout, uint32_t delta_ms)
{
    if (!s_norm.enabled || !anim_inout) return false;
    s_norm_acc_ms += delta_ms;
    uint32_t threshold_ms = (uint32_t)s_norm.interval_seconds * 1000U;
    if (threshold_ms < 5000) threshold_ms = 5000;
    if (s_norm_acc_ms < threshold_ms) return false;
    s_norm_acc_ms = 0;

    animation_type_t cur = *anim_inout;
    animation_type_t pick = cur;
    /* Skip ANIM_CUSTOM which has its own UX */
    int range = ANIM_TYPE_MAX - 1;
    for (int tries = 0; tries < 8; tries++) {
        pick = (animation_type_t)(esp_random() % range);
        if (!s_norm.no_repeat || pick != cur) break;
    }
    if (pick != cur) {
        *anim_inout = pick;
        ESP_LOGI(TAG, "normal -> %d", pick);
        return true;
    }
    return false;
}
