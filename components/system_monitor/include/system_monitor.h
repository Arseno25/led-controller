#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t uptime_ms;
    uint32_t free_heap;
    uint32_t min_free_heap;
    uint32_t largest_free_block;

    uint32_t render_frames;
    uint32_t render_last_ms;
    uint32_t render_max_ms;
    uint32_t render_avg_ms_x100;
    uint32_t render_stack_free_words;

    uint8_t wifi_clients;
    bool audio_running;

    bool config_save_pending;
    uint32_t config_save_count;
    esp_err_t config_last_save_err;
} system_monitor_snapshot_t;

void system_monitor_init(void);
void system_monitor_record_render(uint32_t frame_ms);
void system_monitor_set_wifi_clients(uint8_t clients);
void system_monitor_set_audio_running(bool running);
void system_monitor_set_config_save_state(bool pending, uint32_t save_count, esp_err_t last_err);
void system_monitor_get_snapshot(system_monitor_snapshot_t *out);

#ifdef __cplusplus
}
#endif
