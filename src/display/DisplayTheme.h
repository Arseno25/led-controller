#pragma once

#include <stdint.h>
#include "board_profile.h"

#define DISPLAY_TFT_WIDTH       240
#define DISPLAY_TFT_HEIGHT      240

/* Many GC9A01 modules label SPI clock/data as SCL/SDA.
 * These are not I2C pins:
 *   SCL = SPI SCLK/SCK
 *   SDA = SPI MOSI/DIN
 */
#define DISPLAY_TFT_SCL_PIN     BOARD_DEFAULT_TFT_SCLK_PIN
#define DISPLAY_TFT_SDA_PIN     BOARD_DEFAULT_TFT_MOSI_PIN
#define DISPLAY_TFT_SCLK_PIN    DISPLAY_TFT_SCL_PIN
#define DISPLAY_TFT_MOSI_PIN    DISPLAY_TFT_SDA_PIN
#define DISPLAY_TFT_CS_PIN      BOARD_DEFAULT_TFT_CS_PIN
#define DISPLAY_TFT_DC_PIN      BOARD_DEFAULT_TFT_DC_PIN
#define DISPLAY_TFT_RST_PIN     BOARD_DEFAULT_TFT_RST_PIN

#define DISPLAY_SPI_FREQUENCY   40000000
#define DISPLAY_FRAME_MS        33

/* 0=normal, 1=90deg, 2=180deg, 3=270deg. */
#define DISPLAY_ROTATION        0
/* Toggle these if the module is mirrored after rotation. */
#define DISPLAY_FLIP_X          0
#define DISPLAY_FLIP_Y          0

#if DISPLAY_ROTATION == 0
#define DISPLAY_MADCTL          0x08
#elif DISPLAY_ROTATION == 1
#define DISPLAY_MADCTL          0x68
#elif DISPLAY_ROTATION == 2
#define DISPLAY_MADCTL          0xC8
#else
#define DISPLAY_MADCTL          0xA8
#endif

#if DISPLAY_FLIP_X
#undef DISPLAY_MADCTL
#if DISPLAY_ROTATION == 0
#define DISPLAY_MADCTL          0x48
#elif DISPLAY_ROTATION == 1
#define DISPLAY_MADCTL          0x28
#elif DISPLAY_ROTATION == 2
#define DISPLAY_MADCTL          0x88
#else
#define DISPLAY_MADCTL          0xE8
#endif
#endif

#if DISPLAY_FLIP_Y
#undef DISPLAY_MADCTL
#if DISPLAY_ROTATION == 0
#define DISPLAY_MADCTL          0x88
#elif DISPLAY_ROTATION == 1
#define DISPLAY_MADCTL          0xE8
#elif DISPLAY_ROTATION == 2
#define DISPLAY_MADCTL          0x48
#else
#define DISPLAY_MADCTL          0x28
#endif
#endif

typedef struct {
    uint16_t bg;
    uint16_t panel;
    uint16_t text;
    uint16_t muted;
    uint16_t cyan;
    uint16_t purple;
    uint16_t green;
    uint16_t orange;
    uint16_t red;
} display_theme_colors_t;

static inline uint16_t display_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static const display_theme_colors_t DISPLAY_THEME_NEON_DARK_COLORS = {
    0x0008,
    0x0863,
    0xEFFF,
    0x7BEF,
    0x07FF,
    0xA15F,
    0x07E0,
    0xFD20,
    0xF986,
};

/*
 * Arduino/TFT_eSPI setup equivalent for this wiring:
 *   #define GC9A01_DRIVER
 *   #define TFT_WIDTH  240
 *   #define TFT_HEIGHT 240
 *   #define TFT_MOSI   23
 *   #define TFT_SCLK   18
 *   #define TFT_CS     15
 *   #define TFT_DC     2
 *   #define TFT_RST    4
 *   #define LOAD_GLCD
 *   #define SPI_FREQUENCY 40000000
 *
 * This ESP-IDF project uses a native SPI GC9A01 path so LED GPIO5 and
 * INMP441 GPIO26/25/33 stay untouched.
 */
