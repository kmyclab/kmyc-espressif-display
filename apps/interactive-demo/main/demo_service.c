#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "kmyc_display.h"
#include "demo_service.h"

static const char *TAG = "demo_service";
static int64_t s_wake_at;
static QueueHandle_t s_actions, s_brightness;
static SemaphoreHandle_t s_model_lock;
static demo_model_t s_model;

demo_model_t demo_service_snapshot(void)
{
    demo_model_t copy;
    xSemaphoreTake(s_model_lock, portMAX_DELAY);
    copy = s_model;
    xSemaphoreGive(s_model_lock);
    return copy;
}

static void history(demo_model_t *model, const char *name, esp_err_t error)
{
    memmove(model->history[0], model->history[1], 5 * sizeof(model->history[0]));
    snprintf(model->history[5], sizeof(model->history[5]), "%" PRId64 "ms %s: %s",
             esp_timer_get_time() / 1000, name, esp_err_to_name(error));
}

static void service_task(void *context)
{
    (void)context;
    demo_model_t model = {0};
    esp_err_t error = kmyc_display_start(false);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Display startup failed: %s; no retry", esp_err_to_name(error));
        model.boot_failed = true;
        model.error = error;
        xSemaphoreTake(s_model_lock, portMAX_DELAY);
        s_model = model;
        xSemaphoreGive(s_model_lock);
        vTaskDelete(NULL);
        return;
    }
    model.error = kmyc_touch_start();
    history(&model, "touch startup", model.error);
    if (kmyc_adapter_get_info()->controller_online) {
        error = kmyc_display_set_brightness(60);
        if (error != ESP_OK) {
            model.error = error;
        }
    }
    model.ready = true;
    int64_t touch_at = 0, status_at = 0, brightness_at = 0;
    esp_err_t last_touch_error = ESP_OK;
    for (;;) {
        int64_t now = esp_timer_get_time();
        unsigned id;
        if (xQueueReceive(s_actions, &id, 0) == pdTRUE) {
            switch (id) {
            case 1:
                model.error = kmyc_display_sleep();
                if (kmyc_display_is_asleep()) {
                    s_wake_at = esp_timer_get_time() + 3000000;
                }
                model.report.count = 0;
                break;
            case 2:
                model.error = kmyc_display_wake();
                s_wake_at = 0;
                model.report.count = 0;
                break;
            case 3:
                model.error = kmyc_adapter_touch_reset();
                model.report.count = 0;
                break;
            case 4:
                model.error = kmyc_touch_reprobe();
                break;
            case 5:
                model.error = kmyc_adapter_clear_diagnostics();
                break;
            case 6:
                esp_restart();
                break;
            default:
                break;
            }
            static const char *actions[] = {"unknown",       "sleep", "wake",   "touch reset",
                                            "touch reprobe", "clear", "restart"};
            history(&model, id < 7 ? actions[id] : "unknown", model.error);
        }
        if (s_wake_at && now >= s_wake_at) {
            model.error = kmyc_display_wake();
            s_wake_at = 0;
            model.report.count = 0;
            history(&model, "automatic wake", model.error);
        }
        uint8_t percent;
        if (now >= brightness_at && xQueueReceive(s_brightness, &percent, 0) == pdTRUE) {
            model.error = kmyc_display_set_brightness(percent);
            history(&model, "brightness", model.error);
            brightness_at = esp_timer_get_time() + 30000;
        }
        model.asleep = kmyc_display_is_asleep();
        if (!model.asleep && now >= touch_at) {
            bool updated = false;
            kmyc_touch_report_t report;
            error = kmyc_touch_read(&report, &updated);
            if (error != last_touch_error) {
                history(&model, "touch poll", error);
            }
            last_touch_error = error;
            if (error != ESP_OK) {
                model.report.count = 0;
                model.error = error;
            } else if (updated) {
                model.report = report;
                model.report_sequence++;
            }
            touch_at = esp_timer_get_time() + 20000;
        }
        if (now >= status_at) {
            kmyc_adapter_refresh();
            status_at = esp_timer_get_time() + 1000000;
        }
        model.adapter = *kmyc_adapter_get_info();
        model.touch = *kmyc_touch_get_info();
        xSemaphoreTake(s_model_lock, portMAX_DELAY);
        s_model = model;
        xSemaphoreGive(s_model_lock);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t demo_service_queue_action(unsigned action)
{
    return xQueueSend(s_actions, &action, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t demo_service_queue_brightness(uint8_t percent)
{
    return xQueueOverwrite(s_brightness, &percent) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t demo_service_start(void)
{
    s_model_lock = xSemaphoreCreateMutex();
    s_actions = xQueueCreate(8, sizeof(unsigned));
    s_brightness = xQueueCreate(1, sizeof(uint8_t));
    if (!s_model_lock || !s_actions || !s_brightness) {
        return ESP_ERR_NO_MEM;
    }
    return xTaskCreate(service_task, "device_service", 8192, NULL, 5, NULL) == pdPASS
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}
