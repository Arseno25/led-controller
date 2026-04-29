#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_controller_init(void);
esp_err_t app_controller_start(void);
esp_err_t app_controller_apply_config(void);
esp_err_t app_controller_factory_reset(void);

#ifdef __cplusplus
}
#endif
