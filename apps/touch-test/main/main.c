#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "kmyc_display.h"
#include "kmyc_touch.h"
#include "touch_view.h"

static const char *TAG = "touch_test";

void app_main(void)
{
    const kmyc_display_info_t *display = kmyc_display_get_info();
    ESP_LOGI(TAG, "Starting touch diagnostics with %s", display->model);
    ESP_ERROR_CHECK(kmyc_display_start(false));
    ESP_ERROR_CHECK(touch_view_start(display));
    ESP_ERROR_CHECK(kmyc_touch_start());

    uint8_t previous_count = 0;
    while (true) {
        kmyc_touch_report_t report;
        bool updated = false;
        ESP_ERROR_CHECK(kmyc_touch_read(&report, &updated));
        if (updated) {
            ESP_ERROR_CHECK(touch_view_update(&report));
        }
        if (updated && report.count == 0 && previous_count > 0) {
            ESP_LOGI(TAG, "touch released");
        } else if (updated) {
            for (uint8_t index = 0; index < report.count; index++) {
                const kmyc_touch_point_t *point = &report.points[index];
                ESP_LOGI(TAG, "touch %u/%u: id=%u x=%u y=%u strength=%u",
                         index + 1, report.count, point->id, point->x, point->y,
                         point->strength);
            }
        }
        if (updated) {
            previous_count = report.count;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
