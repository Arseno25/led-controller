#pragma once

#include "sdkconfig.h"

/*
 * Target-aware default pins.
 *
 * ESP32 keeps the original wiring used by the current controller board.
 * Other targets use GPIOs that exist on the chip and avoid SPI flash pins.
 * Users can still override LED/audio pins from the Web UI and NVS config.
 */

#if defined(CONFIG_IDF_TARGET_ESP32)
#define BOARD_TARGET_NAME              "esp32"
#define BOARD_DEFAULT_LED_PIN          5
#define BOARD_DEFAULT_I2S_BCLK_PIN     26
#define BOARD_DEFAULT_I2S_WS_PIN       25
#define BOARD_DEFAULT_I2S_DATA_PIN     33
#define BOARD_DEFAULT_TFT_SCLK_PIN     18
#define BOARD_DEFAULT_TFT_MOSI_PIN     23
#define BOARD_DEFAULT_TFT_CS_PIN       15
#define BOARD_DEFAULT_TFT_DC_PIN       2
#define BOARD_DEFAULT_TFT_RST_PIN      4

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define BOARD_TARGET_NAME              "esp32s3"
#define BOARD_DEFAULT_LED_PIN          5
#define BOARD_DEFAULT_I2S_BCLK_PIN     6
#define BOARD_DEFAULT_I2S_WS_PIN       7
#define BOARD_DEFAULT_I2S_DATA_PIN     15
#define BOARD_DEFAULT_TFT_SCLK_PIN     12
#define BOARD_DEFAULT_TFT_MOSI_PIN     11
#define BOARD_DEFAULT_TFT_CS_PIN       10
#define BOARD_DEFAULT_TFT_DC_PIN       9
#define BOARD_DEFAULT_TFT_RST_PIN      8

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define BOARD_TARGET_NAME              "esp32s2"
#define BOARD_DEFAULT_LED_PIN          5
#define BOARD_DEFAULT_I2S_BCLK_PIN     6
#define BOARD_DEFAULT_I2S_WS_PIN       7
#define BOARD_DEFAULT_I2S_DATA_PIN     15
#define BOARD_DEFAULT_TFT_SCLK_PIN     12
#define BOARD_DEFAULT_TFT_MOSI_PIN     11
#define BOARD_DEFAULT_TFT_CS_PIN       10
#define BOARD_DEFAULT_TFT_DC_PIN       9
#define BOARD_DEFAULT_TFT_RST_PIN      8

#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define BOARD_TARGET_NAME              "esp32c3"
#else
#define BOARD_TARGET_NAME              "esp32c6"
#endif
#define BOARD_DEFAULT_LED_PIN          5
#define BOARD_DEFAULT_I2S_BCLK_PIN     18
#define BOARD_DEFAULT_I2S_WS_PIN       19
#define BOARD_DEFAULT_I2S_DATA_PIN     3
#define BOARD_DEFAULT_TFT_SCLK_PIN     6
#define BOARD_DEFAULT_TFT_MOSI_PIN     7
#define BOARD_DEFAULT_TFT_CS_PIN       10
#define BOARD_DEFAULT_TFT_DC_PIN       2
#define BOARD_DEFAULT_TFT_RST_PIN      4

#else
#define BOARD_TARGET_NAME              "generic"
#define BOARD_DEFAULT_LED_PIN          5
#define BOARD_DEFAULT_I2S_BCLK_PIN     6
#define BOARD_DEFAULT_I2S_WS_PIN       7
#define BOARD_DEFAULT_I2S_DATA_PIN     3
#define BOARD_DEFAULT_TFT_SCLK_PIN     12
#define BOARD_DEFAULT_TFT_MOSI_PIN     11
#define BOARD_DEFAULT_TFT_CS_PIN       10
#define BOARD_DEFAULT_TFT_DC_PIN       9
#define BOARD_DEFAULT_TFT_RST_PIN      8
#endif
