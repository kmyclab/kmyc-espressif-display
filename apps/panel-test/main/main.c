#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "kmyc_display.h"

static const char *TAG = "panel_test";

void app_main(void)
{
    const kmyc_display_info_t *info = kmyc_display_get_info();
    ESP_LOGI(TAG, "%s: %dx%d, %d active lanes, nominal %.2f Hz",
             info->model, info->width, info->height, info->active_lanes,
             info->nominal_refresh_hz);

#ifdef CONFIG_KMYC_PANEL_INTERNAL_BIST
    const bool internal_bist = true;
#else
    const bool internal_bist = false;
#endif
    ESP_ERROR_CHECK(kmyc_display_start(internal_bist));
    ESP_LOGW(TAG, "Experimental diagnostic; touch is not initialized");

    unsigned long pattern_number = 0;
    while (true) {
        if (internal_bist) {
            ESP_LOGI(TAG, "BIST heartbeat #%lu (DPI video off)", pattern_number);
        } else {
            const bool horizontal = (pattern_number & 1U) == 0;
            ESP_ERROR_CHECK(kmyc_display_set_test_pattern(horizontal));
            ESP_LOGI(TAG, "Pattern #%lu: %s color bars", pattern_number,
                     horizontal ? "horizontal" : "vertical");
        }
        pattern_number++;
        vTaskDelay(pdMS_TO_TICKS(CONFIG_KMYC_PATTERN_PERIOD_MS));
    }
}
