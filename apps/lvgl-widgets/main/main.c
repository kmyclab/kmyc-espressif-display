// SPDX-License-Identifier: GPL-2.0-only
#include "esp_log.h"
#include "lvgl_port.h"

void app_main(void)
{
    /* This task owns LVGL, display startup and all touch/bus access. */
    esp_err_t error = lvgl_port_run();
    ESP_LOGE("lvgl_widgets", "Startup stopped: %s; no automatic retry",
             esp_err_to_name(error));
}
