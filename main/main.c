#include "esp_err.h"
#include "esp_log.h"

#include "app_controller.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(app_controller_init());
    ESP_ERROR_CHECK(app_controller_start());
    ESP_LOGI(TAG, "app_main done");
}
