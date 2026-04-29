#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_SPECTRUM_BANDS 16

typedef struct {
    uint8_t bars[DISPLAY_SPECTRUM_BANDS];
    uint8_t bass;
    uint8_t mid;
    uint8_t treble;
    uint8_t level;
    bool beat;
} display_spectrum_view_t;
