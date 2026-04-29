#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool power;
    const char *mode;
    const char *effect;
    uint8_t brightness;
    uint8_t speed;
    uint8_t sensitivity;
    bool wifi_connected;
    int fps;
    uint8_t audio_level;
    uint8_t palette_id;
    bool autoplay;
} display_status_view_t;
