#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_AP_SSID     "PixelController-Setup"
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_MAX_CONN 4

esp_err_t wifi_service_start_ap(void);

#ifdef __cplusplus
}
#endif
