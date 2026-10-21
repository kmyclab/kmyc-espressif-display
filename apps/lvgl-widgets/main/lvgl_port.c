// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <stdlib.h>
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "kmyc_display.h"
#include "kmyc_touch.h"
#include "lvgl_port.h"

static const char *TAG = "lvgl_widgets";
static bool s_touch_enabled;
static unsigned s_touch_errors;
static kmyc_touch_report_t s_report;
static lv_point_t s_last_point;

static uint32_t milliseconds(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool flush_done(void *context)
{
    TaskHandle_t owner = (TaskHandle_t)context;
    if (xPortInIsrContext()) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(owner, &woken);
        return woken == pdTRUE;
    }
    xTaskNotifyGive(owner);
    return false;
}

static void flush_wait(lv_display_t *display)
{
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200))) {
        lv_display_flush_ready(display);
    } else {
        /* Keep both buffers alive: a lost completion does not cancel DMA. */
        ESP_LOGE(TAG, "Display copy timeout; UI stopped without reusing the buffer");
        vTaskSuspend(NULL);
    }
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    (void)display;
    ulTaskNotifyTake(pdTRUE, 0);
    esp_err_t error = kmyc_display_draw_rgb888(area->x1, area->y1,
                                             area->x2 + 1, area->y2 + 1, pixels);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Draw failed: %s; UI stopped", esp_err_to_name(error));
        vTaskSuspend(NULL);
    }
}

static void touch_read(lv_indev_t *input, lv_indev_data_t *data)
{
    (void)input;
    if (s_touch_enabled) {
        kmyc_touch_report_t report;
        bool updated = false;
        esp_err_t error = kmyc_touch_read(&report, &updated);
        if (error == ESP_OK) {
            s_touch_errors = 0;
            /* No new Goodix report means keep the last press, not release it. */
            if (updated) {
                s_report = report;
            }
        } else {
            s_report.count = 0;
            if (++s_touch_errors >= 3) {
                s_touch_enabled = false;
                ESP_LOGE(TAG, "Touch polling stopped after three errors; check wiring and restart");
            }
        }
    }
    data->state = s_report.count ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (s_report.count) {
        s_last_point.x = s_report.points[0].x;
        s_last_point.y = s_report.points[0].y;
    }
    data->point = s_last_point;
}

esp_err_t lvgl_port_run(void)
{
    ESP_RETURN_ON_ERROR(kmyc_display_start(false), TAG, "Display startup");
    esp_err_t touch_error = kmyc_touch_start();
    s_touch_enabled = touch_error == ESP_OK;
    if (!s_touch_enabled) {
        ESP_LOGW(TAG, "Touch unavailable: %s; widgets will still be displayed",
                 esp_err_to_name(touch_error));
    }

    const kmyc_display_info_t *info = kmyc_display_get_info();
    ESP_RETURN_ON_FALSE(info->width > 0 && info->height > 0 &&
                        (size_t)info->width <= SIZE_MAX / (40U * 3U),
                        ESP_ERR_INVALID_SIZE, TAG, "Invalid display dimensions");
    size_t rows = info->height < 40 ? (size_t)info->height : 40U;
    size_t bytes = (size_t)info->width * rows * 3U;
    lv_init();
    lv_tick_set_cb(milliseconds);
    lv_display_t *display = lv_display_create(info->width, info->height);
    void *buffer1 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buffer2 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!display || !buffer1 || !buffer2) {
        free(buffer1);
        free(buffer2);
        if (display) {
            lv_display_delete(display);
        }
        return ESP_ERR_NO_MEM;
    }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, buffer1, buffer2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_flush_wait_cb(display, flush_wait);
    ESP_RETURN_ON_ERROR(kmyc_display_set_flush_done_callback(flush_done,
                        (void *)xTaskGetCurrentTaskHandle()), TAG, "Display completion callback");

    lv_indev_t *input = lv_indev_create();
    ESP_RETURN_ON_FALSE(input, ESP_ERR_NO_MEM, TAG, "LVGL input allocation");
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(input, display);
    lv_indev_set_read_cb(input, touch_read);
    lv_timer_set_period(lv_indev_get_read_timer(input), 20);

    /* Use upstream widgets/assets directly; no copied or modified demo source. */
    lv_demo_widgets();
    if (!s_touch_enabled) {
        lv_obj_t *notice = lv_label_create(lv_layer_top());
        lv_label_set_text(notice, "Touch unavailable - check wiring and restart");
        lv_obj_set_style_bg_color(notice, lv_color_hex(0x8b1e1e), 0);
        lv_obj_set_style_bg_opa(notice, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(notice, lv_color_white(), 0);
        lv_obj_align(notice, LV_ALIGN_BOTTOM_LEFT, 4, -4);
    }
    lv_refr_now(display);
    ESP_RETURN_ON_ERROR(kmyc_display_set_brightness(60), TAG, "Bridge brightness");
    ESP_LOGI(TAG, "LVGL %d.%d.%d Widgets Demo ready: %dx%d RGB888, touch %s",
             LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH,
             info->width, info->height, s_touch_enabled ? "enabled" : "unavailable");
    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms > 20) {
            wait_ms = 20;
        }
        if (wait_ms < 1) {
            wait_ms = 1;
        }
        vTaskDelay(pdMS_TO_TICKS(wait_ms) + 1);
    }
}
