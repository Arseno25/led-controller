#pragma once

#include "esp_err.h"
#include "config_manager.h"
#include "audio_processor.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t reactive_engine_init(const audio_config_t *cfg);
esp_err_t reactive_engine_start(void);
esp_err_t reactive_engine_stop(void);
esp_err_t reactive_engine_set_config(const audio_config_t *cfg);
esp_err_t reactive_engine_get_features(audio_features_t *out);
bool      reactive_engine_is_running(void);

#ifdef __cplusplus
}
#endif
