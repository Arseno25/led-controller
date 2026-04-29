#include "reactive_engine.h"
#include "audio_input.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rx_engine";

#define TASK_STACK 4096
#define TASK_PRIO  5

static TaskHandle_t s_task = NULL;
static volatile bool s_run = false;
static int16_t *s_buf = NULL;
static size_t   s_buf_n = 0;

static void engine_task(void *arg)
{
    while (s_run) {
        size_t got = 0;
        if (audio_input_read_samples(s_buf, s_buf_n, &got) == ESP_OK && got > 0) {
            audio_processor_process(s_buf, got, NULL);
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t reactive_engine_init(const audio_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    esp_err_t err = audio_input_init(cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "audio_input init fail"); return err; }
    err = audio_processor_init(cfg);
    if (err != ESP_OK) { ESP_LOGE(TAG, "audio_proc init fail"); return err; }

    s_buf_n = cfg->buffer_size > 0 ? cfg->buffer_size : 256;
    int16_t *p = (int16_t *)realloc(s_buf, s_buf_n * sizeof(int16_t));
    if (!p) return ESP_ERR_NO_MEM;
    s_buf = p;
    return ESP_OK;
}

esp_err_t reactive_engine_start(void)
{
    if (s_task) return ESP_OK;
    if (!audio_input_is_initialized()) return ESP_ERR_INVALID_STATE;
    s_run = true;
    if (xTaskCreate(engine_task, "rx_engine", TASK_STACK, NULL, TASK_PRIO, &s_task) != pdPASS) {
        s_run = false;
        ESP_LOGE(TAG, "task create fail");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}

esp_err_t reactive_engine_stop(void)
{
    if (!s_task) return ESP_OK;
    s_run = false;
    for (int i = 0; i < 30 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "stopped");
    return ESP_OK;
}

esp_err_t reactive_engine_set_config(const audio_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    bool was = (s_task != NULL);
    if (was) reactive_engine_stop();
    esp_err_t err = audio_input_reconfigure(cfg);
    if (err != ESP_OK) return err;
    err = audio_processor_set_config(cfg);
    if (err != ESP_OK) return err;
    s_buf_n = cfg->buffer_size > 0 ? cfg->buffer_size : 256;
    int16_t *p = (int16_t *)realloc(s_buf, s_buf_n * sizeof(int16_t));
    if (!p) return ESP_ERR_NO_MEM;
    s_buf = p;
    if (was) return reactive_engine_start();
    return ESP_OK;
}

esp_err_t reactive_engine_get_features(audio_features_t *out)
{
    return audio_processor_get_features(out);
}

bool reactive_engine_is_running(void) { return s_task != NULL; }
