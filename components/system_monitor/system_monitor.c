#include "system_monitor.h"

#include <string.h>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static system_monitor_snapshot_t s_state;

void system_monitor_init(void)
{
    portENTER_CRITICAL(&s_mux);
    memset(&s_state, 0, sizeof(s_state));
    s_state.config_last_save_err = ESP_OK;
    portEXIT_CRITICAL(&s_mux);
}

void system_monitor_record_render(uint32_t frame_ms)
{
    UBaseType_t stack_free = uxTaskGetStackHighWaterMark(NULL);

    portENTER_CRITICAL(&s_mux);
    s_state.render_frames++;
    s_state.render_last_ms = frame_ms;
    if (frame_ms > s_state.render_max_ms) s_state.render_max_ms = frame_ms;
    if (s_state.render_avg_ms_x100 == 0) {
        s_state.render_avg_ms_x100 = frame_ms * 100U;
    } else {
        s_state.render_avg_ms_x100 =
            (s_state.render_avg_ms_x100 * 95U + frame_ms * 100U * 5U) / 100U;
    }
    s_state.render_stack_free_words = (uint32_t)stack_free;
    portEXIT_CRITICAL(&s_mux);
}

void system_monitor_set_wifi_clients(uint8_t clients)
{
    portENTER_CRITICAL(&s_mux);
    s_state.wifi_clients = clients;
    portEXIT_CRITICAL(&s_mux);
}

void system_monitor_set_audio_running(bool running)
{
    portENTER_CRITICAL(&s_mux);
    s_state.audio_running = running;
    portEXIT_CRITICAL(&s_mux);
}

void system_monitor_set_config_save_state(bool pending, uint32_t save_count, esp_err_t last_err)
{
    portENTER_CRITICAL(&s_mux);
    s_state.config_save_pending = pending;
    s_state.config_save_count = save_count;
    s_state.config_last_save_err = last_err;
    portEXIT_CRITICAL(&s_mux);
}

void system_monitor_get_snapshot(system_monitor_snapshot_t *out)
{
    if (!out) return;

    portENTER_CRITICAL(&s_mux);
    *out = s_state;
    portEXIT_CRITICAL(&s_mux);

    out->uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    out->free_heap = esp_get_free_heap_size();
    out->min_free_heap = esp_get_minimum_free_heap_size();
    out->largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
}
