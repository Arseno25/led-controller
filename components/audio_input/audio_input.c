#include "audio_input.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "audio_in";

static bool s_init = false;
static i2s_chan_handle_t s_i2s_rx = NULL;
static audio_config_t s_cfg;
static int32_t *s_tmp = NULL;
static size_t   s_tmp_cap = 0;

static esp_err_t i2s_open(const audio_config_t *cfg)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num  = 4;
    chan_cfg.dma_frame_num = cfg->buffer_size > 0 ? cfg->buffer_size : 256;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_i2s_rx);
    if (err != ESP_OK) { ESP_LOGE(TAG, "new_channel: %s", esp_err_to_name(err)); return err; }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)cfg->i2s_bclk_pin,
            .ws   = (gpio_num_t)cfg->i2s_ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)cfg->i2s_data_pin,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(s_i2s_rx, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "init_std: %s", esp_err_to_name(err));
        i2s_del_channel(s_i2s_rx); s_i2s_rx = NULL;
        return err;
    }

    err = i2s_channel_enable(s_i2s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable fail");
        i2s_del_channel(s_i2s_rx); s_i2s_rx = NULL;
        return err;
    }

    ESP_LOGI(TAG, "INMP441 OK bclk=%d ws=%d din=%d rate=%lu buf=%u",
             cfg->i2s_bclk_pin, cfg->i2s_ws_pin, cfg->i2s_data_pin,
             (unsigned long)cfg->sample_rate, (unsigned)cfg->buffer_size);
    return ESP_OK;
}

static void i2s_close(void)
{
    if (s_i2s_rx) {
        i2s_channel_disable(s_i2s_rx);
        i2s_del_channel(s_i2s_rx);
        s_i2s_rx = NULL;
    }
}

static esp_err_t ensure_tmp(size_t n)
{
    size_t need = n * sizeof(int32_t);
    if (need <= s_tmp_cap) return ESP_OK;
    int32_t *p = (int32_t *)realloc(s_tmp, need);
    if (!p) return ESP_ERR_NO_MEM;
    s_tmp = p; s_tmp_cap = need;
    return ESP_OK;
}

esp_err_t audio_input_init(const audio_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (s_init) return ESP_OK;

    s_cfg = *cfg;
    esp_err_t err = i2s_open(&s_cfg);
    if (err != ESP_OK) return err;
    s_init = true;
    return ESP_OK;
}

esp_err_t audio_input_deinit(void)
{
    if (!s_init) return ESP_OK;
    i2s_close();
    free(s_tmp); s_tmp = NULL; s_tmp_cap = 0;
    s_init = false;
    return ESP_OK;
}

esp_err_t audio_input_reconfigure(const audio_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (s_init) {
        i2s_close();
        s_init = false;
    }
    return audio_input_init(cfg);
}

esp_err_t audio_input_read_samples(int16_t *buffer, size_t buffer_size, size_t *samples_read)
{
    if (!s_init || !buffer || !samples_read) return ESP_ERR_INVALID_ARG;

    esp_err_t err = ensure_tmp(buffer_size);
    if (err != ESP_OK) { *samples_read = 0; return err; }

    size_t bytes_read = 0;
    err = i2s_channel_read(s_i2s_rx, s_tmp, buffer_size * sizeof(int32_t),
                           &bytes_read, pdMS_TO_TICKS(100));
    if (err != ESP_OK) {
        *samples_read = 0;
        if (err == ESP_ERR_TIMEOUT) return ESP_OK;
        return err;
    }

    size_t count = bytes_read / sizeof(int32_t);
    for (size_t i = 0; i < count; i++) {
        /*
         * INMP441 outputs 24-bit data left-justified in 32-bit.
         * Shift by 14 instead of 16 for a small digital preamp; clamp protects
         * loud input while improving sensitivity for quiet music/speech pickup.
         */
        int32_t sample = s_tmp[i] >> 14;
        if (sample > INT16_MAX) sample = INT16_MAX;
        if (sample < INT16_MIN) sample = INT16_MIN;
        buffer[i] = (int16_t)sample;
    }
    *samples_read = count;
    return ESP_OK;
}

bool audio_input_is_initialized(void) { return s_init; }
