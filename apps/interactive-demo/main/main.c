#include "demo_service.h"
#include "demo_ui.h"
#include "esp_log.h"

void app_main(void)
{
    esp_err_t error = demo_service_start();
    if (error != ESP_OK) {
        ESP_LOGE("interactive_demo", "Service start failed: %s", esp_err_to_name(error));
        return;
    }
    error = demo_ui_start();
    if (error != ESP_OK) {
        ESP_LOGE("interactive_demo", "UI start failed: %s", esp_err_to_name(error));
    }
}
