#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "kmyc_display.h"
#include "demo_service.h"
#include "demo_ui.h"

static const char *TAG = "demo_ui";
static lv_obj_t *s_system, *s_descriptor, *s_lifecycle, *s_touch_label, *s_duty_label;
static lv_obj_t *s_preview, *s_touch_area, *s_points[5], *s_targets[5];
static kmyc_touch_report_t s_report;
static uint32_t s_multi_reports, s_edge_reports, s_target_hits, s_max_contacts;
static bool s_target_seen[5];
static esp_err_t s_ui_error;
static uint32_t s_flush_errors;
static TaskHandle_t s_lvgl_task;
static demo_model_t s_ui_model;
static lv_obj_t *s_controller_widgets[10];
static unsigned s_controller_widget_count;
static lv_obj_t *s_tabs;

static void tick(void *ctx)
{
    (void)ctx;
    lv_tick_inc(2);
}

static bool flush_done(void *context)
{
    (void)context;
    if (xPortInIsrContext()) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(s_lvgl_task, &woken);
        return woken == pdTRUE;
    }
    xTaskNotifyGive(s_lvgl_task);
    return false;
}

static void flush_wait(lv_display_t *display)
{
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) {
        lv_display_flush_ready(display);
    } else {
        /* Never recycle a buffer without completion. Stop UI on lost callback,
         * keeping both buffers allocated; service task continues diagnostics. */
        ESP_LOGE(TAG, "DPI copy completion timeout; stopping UI without reusing buffer");
        vTaskSuspend(NULL);
    }
}

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    ulTaskNotifyTake(pdTRUE, 0);
    if (!s_ui_model.asleep) {
        esp_err_t error =
            kmyc_display_draw_rgb888(area->x1, area->y1, area->x2 + 1, area->y2 + 1, pixels);
        if (error != ESP_OK) {
            s_ui_error = error;
            s_flush_errors++;
            lv_display_flush_ready(display); /* Submission rejected; no transfer owns it. */
        }
    } else {
        lv_display_flush_ready(display);
    }
}

static void pointer_read(lv_indev_t *device, lv_indev_data_t *data)
{
    (void)device;
    data->state = s_report.count ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    if (s_report.count) {
        data->point.x = s_report.points[0].x;
        data->point.y = s_report.points[0].y;
    }
}

static lv_obj_t *label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_width(obj, LV_PCT(100));
    return obj;
}

static void action(lv_event_t *event)
{
    unsigned id = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    s_ui_error = demo_service_queue_action(id);
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, unsigned id, bool needs_controller)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_size(obj, 180, 44);
    lv_obj_t *caption = lv_label_create(obj);
    lv_label_set_text(caption, text);
    lv_obj_center(caption);
    lv_obj_add_event_cb(obj, action, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
    if (needs_controller) {
        s_controller_widgets[s_controller_widget_count++] = obj;
        if (!s_ui_model.adapter.controller_online) {
            lv_obj_add_state(obj, LV_STATE_DISABLED);
        }
    }
    return obj;
}

static lv_obj_t *row(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_width(obj, LV_PCT(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_border_width(obj, 0, 0);
    return obj;
}

static void brightness(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int percent = lv_slider_get_value(slider);
    unsigned duty = (percent * 255 + 50) / 100;
    uint8_t requested = percent;
    s_ui_error = demo_service_queue_brightness(requested);
    lv_label_set_text_fmt(s_duty_label, "%d%% / requested raw duty %u (queued)", percent, duty);
}

static void pattern(lv_event_t *event)
{
    unsigned kind = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    lv_obj_clean(s_preview);
    static const uint32_t colors[] = {0xffffff, 0xffff00, 0x00ffff, 0x00ff00,
                                      0xff00ff, 0xff0000, 0x0000ff, 0};
    for (unsigned y = 0; y < 4; y++) {
        for (unsigned x = 0; x < 8; x++) {
            uint32_t color;
            if (kind == 0) {
                color = colors[x];
            } else if (kind == 1) {
                unsigned gray = x * 255 / 7;
                color = gray * 0x010101;
            } else if (kind == 2) {
                color = ((x + y) & 1) ? 0xffffff : 0;
            } else {
                color = 0x183a50;
            }
            lv_obj_t *tile = lv_obj_create(s_preview);
            lv_obj_remove_style_all(tile);
            lv_obj_set_pos(tile, x * 112, y * 45);
            lv_obj_set_size(tile, kind == 3 ? 110 : 112, kind == 3 ? 43 : 45);
            lv_obj_set_style_bg_color(tile, lv_color_hex(color), 0);
            lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
        }
    }
}

static void build_ui(void)
{
    lv_obj_t *tabs = lv_tabview_create(lv_screen_active());
    s_tabs = tabs;
    const char *names[] = {"System", "Display", "Touch", "Lifecycle"};
    lv_obj_t *pages[4];
    for (unsigned i = 0; i < 4; i++) {
        pages[i] = lv_tabview_add_tab(tabs, names[i]);
        lv_obj_set_flex_flow(pages[i], LV_FLEX_FLOW_COLUMN);
    }
    label(pages[0], "WT9932P4-TINY V1.2 / integration under test\n"
                    "Display KMYC-D070-DSI4L-1024X600-A1\n"
                    "Touch KMYC-T070-CTP-I2C-GT911-G01-A1\n"
                    "Adapter wireless-tiny-d070-bridge-v12-dsi2");
    s_system = label(pages[0], "Reading controller...");
    s_descriptor = label(pages[0], "Preset: D070 1024x600 / GT911 / 2 DSI lanes");
    label(pages[1], "1024x600 / RGB888 / 2 lanes / 750 Mbps per lane / DPI 51 MHz\n"
                    "LVGL pattern previews; color and geometry require physical verification.");
    lv_obj_t *patterns = row(pages[1]);
    const char *pattern_names[] = {"RGB bars", "Gray ramp", "Checker", "Grid"};
    for (unsigned i = 0; i < 4; i++) {
        lv_obj_t *obj = lv_button_create(patterns);
        lv_obj_t *text = lv_label_create(obj);
        lv_label_set_text(text, pattern_names[i]);
        lv_obj_add_event_cb(obj, pattern, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    s_preview = lv_obj_create(pages[1]);
    lv_obj_set_size(s_preview, 900, 184);
    lv_obj_set_style_pad_all(s_preview, 0, 0);
    lv_obj_remove_flag(s_preview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *slider = lv_slider_create(pages[1]);
    lv_obj_set_width(slider, 800);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider, brightness, LV_EVENT_RELEASED, NULL);
    s_controller_widgets[s_controller_widget_count++] = slider;
    if (!s_ui_model.adapter.controller_online) {
        lv_obj_add_state(slider, LV_STATE_DISABLED);
    }
    s_duty_label =
        label(pages[1], "Initial brightness: 60% / raw duty 153 (only with compatible controller)");
    lv_obj_t *display_row = row(pages[1]);
    button(display_row, "Sleep / wake in 3s", 1, true);
    button(display_row, "Wake", 2, true);
    s_touch_label = label(pages[2], "Touch polling 20 ms; unavailable touch does not stop display");
    lv_obj_t *touch_row = row(pages[2]);
    button(touch_row, "Reset GT911", 3, true);
    button(touch_row, "Reprobe", 4, false);
    s_touch_area = lv_obj_create(pages[2]);
    lv_obj_set_size(s_touch_area, 900, 260);
    lv_obj_remove_flag(s_touch_area, LV_OBJ_FLAG_SCROLLABLE);
    for (unsigned i = 0; i < 5; i++) {
        s_targets[i] = lv_obj_create(s_touch_area);
        lv_obj_set_size(s_targets[i], 30, 30);
        lv_obj_set_pos(s_targets[i], i == 4 ? 420 : (i & 1 ? 810 : 20),
                       i == 4 ? 100 : (i & 2 ? 190 : 10));
        lv_obj_set_style_radius(s_targets[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_targets[i], lv_color_hex(0x505050), 0);
        s_points[i] = lv_obj_create(s_touch_area);
        lv_obj_set_size(s_points[i], 12, 12);
        lv_obj_align(s_points[i], LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(s_points[i], lv_color_hex(0xff4020), 0);
        lv_obj_add_flag(s_points[i], LV_OBJ_FLAG_HIDDEN);
    }
    label(pages[3],
          "Startup stages: shared bus / probe ABI / PWM0 / LCD reset / GT911 address reset");
    s_lifecycle = label(pages[3], "Starting...");
    lv_obj_t *life_row = row(pages[3]);
    button(life_row, "Clear diagnostics", 5, true);
    button(life_row, "Sleep / wake in 3s", 1, true);
    button(life_row, "Restart P4", 6, false);
    label(pages[3], "No descriptor writes. Never hot-plug display/touch FFCs. Controller loss "
                    "holds last output.");
}

static void poll_touch(void)
{
    /* Labels and tab layout can move the canvas between reports. */
    lv_obj_update_layout(s_touch_area);
    static uint32_t last_sequence;
    bool updated = s_ui_model.report_sequence != last_sequence;
    last_sequence = s_ui_model.report_sequence;
    kmyc_touch_report_t report = s_ui_model.report;
    s_report = report;
    if (report.count > s_max_contacts) {
        s_max_contacts = report.count;
    }
    if (updated && report.count > 1) {
        s_multi_reports++;
    }
    if (updated) {
        for (unsigned i = 0; i < report.count; i++) {
            if (report.points[i].x < 20 || report.points[i].x > 1003 || report.points[i].y < 20 ||
                report.points[i].y > 579) {
                s_edge_reports++;
            }
            for (unsigned target = 0; target < 5 && lv_tabview_get_tab_active(s_tabs) == 2;
                 target++) {
                lv_area_t area;
                lv_obj_get_coords(s_targets[target], &area);
                int dx = (int)report.points[i].x - (area.x1 + area.x2) / 2;
                int dy = (int)report.points[i].y - (area.y1 + area.y2) / 2;
                if (!s_target_seen[target] && dx * dx + dy * dy < 900) {
                    s_target_seen[target] = true;
                    s_target_hits++;
                    lv_obj_set_style_bg_color(s_targets[target], lv_color_hex(0x30b050), 0);
                }
            }
        }
    }
    lv_area_t content;
    lv_obj_get_content_coords(s_touch_area, &content);
    for (unsigned i = 0; i < 5; i++) {
        bool inside = i < s_report.count && s_report.points[i].x >= content.x1 &&
                      s_report.points[i].x <= content.x2 && s_report.points[i].y >= content.y1 &&
                      s_report.points[i].y <= content.y2;
        if (inside) {
            lv_obj_remove_flag(s_points[i], LV_OBJ_FLAG_HIDDEN);
            /* Child positions are relative to the non-scrolling parent's
             * content box, not its outer border. No display scaling is needed. */
            lv_obj_set_pos(s_points[i], s_report.points[i].x - content.x1 -
                                           lv_obj_get_width(s_points[i]) / 2,
                           s_report.points[i].y - content.y1 -
                               lv_obj_get_height(s_points[i]) / 2);
        } else {
            lv_obj_add_flag(s_points[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    const kmyc_touch_info_t *touch = &s_ui_model.touch;
    char points[280] = {0};
    size_t offset = 0;
    for (unsigned i = 0; i < s_report.count && i < 5; i++) {
        offset += snprintf(points + offset, sizeof(points) - offset, "id%u x%u y%u strength%u  ",
                           s_report.points[i].id, s_report.points[i].x, s_report.points[i].y,
                           s_report.points[i].strength);
    }
    lv_label_set_text_fmt(
        s_touch_label,
        "GT%s @0x%02x FW0x%04x  points %u/max%lu  targets %lu/5  edge %lu  multi %lu\n%s",
        touch->product_id, touch->address, touch->firmware_version, s_report.count,
        (unsigned long)s_max_contacts, (unsigned long)s_target_hits, (unsigned long)s_edge_reports,
        (unsigned long)s_multi_reports, points);
}

static void lvgl_task(void *context)
{
    (void)context;
    s_lvgl_task = xTaskGetCurrentTaskHandle();
    do {
        s_ui_model = demo_service_snapshot();
        if (s_ui_model.boot_failed) {
            vTaskDelete(NULL);
            return;
        }
        if (!s_ui_model.ready) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    } while (!s_ui_model.ready);
    lv_init();
    /* Keep objects in LVGL's configured allocator. Only draw buffers require
     * PSRAM; an arbitrary extra pool can exceed the built-in TLSF pool limit. */
    lv_display_t *display = lv_display_create(1024, 600);
    const size_t bytes = 1024 * 40 * 3;
    void *buf1 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    void *buf2 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf1 || !buf2 || !display) {
        ESP_LOGE(TAG, "LVGL allocation failed");
        free(buf1);
        free(buf2);
        vTaskDelete(NULL);
        return;
    }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_flush_wait_cb(display, flush_wait);
    ESP_ERROR_CHECK(kmyc_display_set_flush_done_callback(flush_done, NULL));
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, pointer_read);
    const esp_timer_create_args_t timer_args = {.callback = tick, .name = "lvgl_tick"};
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 2000));
    build_ui();
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    ESP_LOGI(TAG, "LVGL UI ready; two %u-byte PSRAM draw buffers; object heap free %u bytes",
             (unsigned)bytes, (unsigned)memory.free_size);
    int64_t touch_at = 0, status_at = 0;
    for (;;) {
        int64_t now = esp_timer_get_time();
        bool was_asleep = s_ui_model.asleep;
        s_ui_model = demo_service_snapshot();
        if (was_asleep && !s_ui_model.asleep) {
            lv_obj_invalidate(lv_screen_active());
        }
        if (now >= touch_at) {
            poll_touch();
            touch_at = now + 20000;
        }
        if (now >= status_at) {
            const kmyc_adapter_info_t *info = &s_ui_model.adapter;
            for (unsigned i = 0; i < s_controller_widget_count; i++) {
                if (info->controller_online) {
                    lv_obj_remove_state(s_controller_widgets[i], LV_STATE_DISABLED);
                } else {
                    lv_obj_add_state(s_controller_widgets[i], LV_STATE_DISABLED);
                }
            }
            lv_label_set_text(s_system, info->system);
            lv_label_set_text_fmt(s_descriptor, "Descriptor matches preset: %s\n%s",
                                  info->descriptor_match ? "yes" : "no / unavailable",
                                  info->descriptor);
            lv_label_set_text_fmt(
                s_lifecycle,
                "Stages 0x%02lx: bus=%u probe=%u PWM0=%u LCD=%u TP=%u panel/UI=ready\n"
                "%s\nService %s; UI %s; flush errors %lu; status failures "
                "%u\n%s\n%s\n%s\n%s\n%s\n%s",
                (unsigned long)info->completed_stages, !!(info->completed_stages & 1),
                !!(info->completed_stages & 2), !!(info->completed_stages & 4),
                !!(info->completed_stages & 8), !!(info->completed_stages & 16), info->lifecycle,
                esp_err_to_name(s_ui_model.error), esp_err_to_name(s_ui_error),
                (unsigned long)s_flush_errors, info->consecutive_status_failures,
                s_ui_model.history[0], s_ui_model.history[1], s_ui_model.history[2],
                s_ui_model.history[3], s_ui_model.history[4], s_ui_model.history[5]);
            status_at = now + 1000000;
        }
        if (!s_ui_model.asleep) {
            lv_timer_handler();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t demo_ui_start(void)
{
    return xTaskCreate(lvgl_task, "lvgl", 8192, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
