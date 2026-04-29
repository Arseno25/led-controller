#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_input_init(const audio_config_t *cfg);
esp_err_t audio_input_deinit(void);
esp_err_t audio_input_reconfigure(const audio_config_t *cfg);

/* Blocking read. Returns 0 samples on timeout (no error). */
esp_err_t audio_input_read_samples(int16_t *buffer, size_t buffer_size, size_t *samples_read);

bool audio_input_is_initialized(void);

#ifdef __cplusplus
}
#endif
