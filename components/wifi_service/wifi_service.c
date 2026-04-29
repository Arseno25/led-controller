#include "wifi_service.h"

#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "system_monitor.h"

static const char *TAG = "wifi_service";

static bool s_started = false;
static uint8_t s_client_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != WIFI_EVENT) {
        return;
    }
    switch (id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)data;
            if (s_client_count < UINT8_MAX) s_client_count++;
            system_monitor_set_wifi_clients(s_client_count);
            ESP_LOGI(TAG, "client connected aid=%d", e->aid);
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)data;
            if (s_client_count > 0) s_client_count--;
            system_monitor_set_wifi_clients(s_client_count);
            ESP_LOGI(TAG, "client disconnected aid=%d", e->aid);
            break;
        }
        default:
            break;
    }
}

esp_err_t wifi_service_start_ap(void)
{
    if (s_started) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t ap_cfg = {
        .ap = {
            .channel        = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg        = {.required = false},
        },
    };
    strncpy((char *)ap_cfg.ap.ssid, WIFI_AP_SSID, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen(WIFI_AP_SSID);
    strncpy((char *)ap_cfg.ap.password, WIFI_AP_PASSWORD, sizeof(ap_cfg.ap.password));

    if (strlen(WIFI_AP_PASSWORD) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_started = true;
    s_client_count = 0;
    system_monitor_set_wifi_clients(0);
    ESP_LOGI(TAG, "AP started ssid=%s channel=%d ip=192.168.4.1",
             WIFI_AP_SSID, WIFI_AP_CHANNEL);
    return ESP_OK;
}
