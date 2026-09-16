#include <stdio.h>
#include <string.h>
#include "bridge.h"
#include "kmyc_controller.h"
#include "kmyc_i2c.h"
#include "kmyc_touch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "bridge_v12";
static i2c_master_dev_handle_t s_device;
static SemaphoreHandle_t s_lock;
static kmyc_controller_t s_client;
static kmyc_adapter_info_t s_info;

static int transport_read(void *ctx, uint8_t reg, uint8_t *data, size_t length)
{
    esp_err_t error = i2c_master_transmit_receive(ctx, &reg, 1, data, length, 100);
    if (error != ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(5));
        error = i2c_master_transmit_receive(ctx, &reg, 1, data, length, 100);
    }
    return error;
}

static int transport_write(void *ctx, uint8_t reg, const uint8_t *data, size_t length)
{
    if (length > 16) return ESP_ERR_INVALID_SIZE;
    uint8_t packet[17] = {reg};
    memcpy(packet + 1, data, length);
    return i2c_master_transmit(ctx, packet, length + 1, 100);
}

static esp_err_t record(int error, const char *phase)
{
    s_info.last_error = error;
    snprintf(s_info.lifecycle, sizeof(s_info.lifecycle), "%s: %s (%d)",
             phase, error ? "FAILED" : "OK", error);
    if (error) ESP_LOGE(TAG, "%s error %d", phase, error);
    return error ? ESP_FAIL : ESP_OK;
}

static int reconcile(int error)
{
    if (error == KMYC_CONTROLLER_ERR_TRANSPORT) {
        kmyc_controller_response_t response;
        vTaskDelay(pdMS_TO_TICKS(5));
        int read_error = kmyc_controller_read_response(&s_client, &response);
        if (!read_error && response.sequence == s_client.sequence && response.result == 0)
            return 0; /* Read-only confirmation, never resend an ambiguous command. */
    }
    return error;
}

static esp_err_t apply(uint8_t mask, uint8_t value, uint8_t mode_mask, uint8_t mode,
                       bool pwm, uint8_t duty)
{
    if (!s_info.controller_compatible || !s_info.controller_online) return ESP_ERR_NOT_SUPPORTED;
    const kmyc_controller_state_t state = {
        .gpio_update_mask = mask, .gpio_values = value,
        .gpio_mode_update_mask = mode_mask, .gpio_modes = mode,
        .pwm_update = pwm, .pwm_duty = duty,
    };
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    int error = reconcile(kmyc_controller_apply(&s_client, &state, NULL));
    xSemaphoreGive(s_lock);
    return record(error, "APPLY");
}

const kmyc_adapter_info_t *kmyc_adapter_get_info(void) { return &s_info; }
bool kmyc_bridge_has_control(void) { return s_info.controller_compatible && s_info.controller_online; }

esp_err_t kmyc_adapter_refresh(void)
{
    if (!s_info.controller_compatible || !s_info.controller_online) return ESP_ERR_NOT_SUPPORTED;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    kmyc_controller_info_t info;
    int error = kmyc_controller_read_info(&s_client, &info);
    xSemaphoreGive(s_lock);
    if (error) {
        if (++s_info.consecutive_status_failures >= 3) {
            s_info.controller_online = false;
            snprintf(s_info.system,sizeof(s_info.system),"Controller offline: 3 consecutive status failures. Outputs unchanged; control disabled.");
        }
        return record(error, "status read");
    }
    s_info.consecutive_status_failures = 0;
    snprintf(s_info.system, sizeof(s_info.system),
             "KMYC 0x2C  ABI %u.%u  FW %u.%u.%u  HW %u.%u\n"
             "Capabilities 0x%08lx  uptime %lu ms  reset %u\n"
             "Status 0x%02x  error %u  I2C errors %u  sequence %u\n"
             "GPIO latch 0x%02x modes 0x%02x raw IDR 0x%02x (not voltage)\n"
             "PWM driven %u duty %u frequency %u Hz\n"
             "Descriptor source %u generation %lu length %u",
             info.protocol_major, info.protocol_minor, info.firmware_major,
             info.firmware_minor, info.firmware_patch, info.hardware_major,
             info.hardware_minor, (unsigned long)info.capabilities,
             (unsigned long)info.uptime_ms, info.reset_cause, info.status,
             info.last_error, info.i2c_error_count, info.last_sequence,
             info.gpio_latch, info.gpio_modes, info.gpio_inputs_raw,
             info.pwm_state & 1, info.pwm_duty, info.pwm_frequency_hz,
             info.descriptor_source, (unsigned long)info.descriptor_generation,
             info.descriptor_length);
    return ESP_OK;
}

static void read_descriptor(void)
{
    kmyc_controller_descriptor_t descriptor;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) return;
    int error = kmyc_controller_read_descriptor(&s_client, &descriptor);
    xSemaphoreGive(s_lock);
    if (error) {
        snprintf(s_info.descriptor, sizeof(s_info.descriptor),
                 "Descriptor invalid/unreadable (%d). Preset remains authoritative.", error);
        return;
    }
    const char *expected_display = "KMYC-D070-DSI4L-1024X600-A1";
    const char *expected_touch = "KMYC-T070-CTP-I2C-GT911-G01-A1";
    bool display_match = false, touch_match = false;
    size_t cursor = 0, written = 0;
    kmyc_controller_tlv_t tlv;
    while ((error = kmyc_controller_tlv_next(&descriptor, &cursor, &tlv)) == 0) {
        if (tlv.type == 1) display_match = tlv.length == strlen(expected_display) &&
            !memcmp(tlv.value, expected_display, tlv.length);
        if (tlv.type == 3) touch_match = tlv.length == strlen(expected_touch) &&
            !memcmp(tlv.value, expected_touch, tlv.length);
        char value[160] = {0};
        if (tlv.type >= 1 && tlv.type <= 4) {
            static const char *names[]={"","Display ID","Panel controller","Touch ID","Touch controller"};
            char safe[109];
            for (unsigned i=0;i<tlv.length;i++) safe[i]=tlv.value[i]>=32 && tlv.value[i]<=126 ? tlv.value[i] : '.';
            safe[tlv.length]=0;
            snprintf(value,sizeof(value),"%s: %s",names[tlv.type],safe);
        } else if (tlv.type==5 && tlv.length==8) {
            snprintf(value,sizeof(value),"Pixels %ux%u, physical %ux%u (zero = unknown)",
                tlv.value[0]|tlv.value[1]<<8,tlv.value[2]|tlv.value[3]<<8,
                tlv.value[4]|tlv.value[5]<<8,tlv.value[6]|tlv.value[7]<<8);
        } else if (tlv.type==6 && tlv.length==4) {
            snprintf(value,sizeof(value),"Interface %u (1=DSI), product lanes %u, format %u (1=RGB888)",
                tlv.value[0],tlv.value[1],tlv.value[2]);
        } else if (tlv.type==7 && tlv.length==1) {
            snprintf(value,sizeof(value),"Default orientation %u",tlv.value[0]);
        } else if (tlv.type==8) {
            size_t used=snprintf(value,sizeof(value),"Touch candidate I2C:");
            for (unsigned i=0;i<tlv.length && used+6<sizeof(value);i++)
                used+=snprintf(value+used,sizeof(value)-used," %02X",tlv.value[i]);
        } else snprintf(value,sizeof(value),"Unknown/opaque TLV %02X length %u",tlv.type,tlv.length);
        if (written < sizeof(s_info.descriptor) - 1) {
            int n = snprintf(s_info.descriptor + written, sizeof(s_info.descriptor) - written,"%s\n",value);
            if (n > 0) written += (size_t)n;
        }
    }
    s_info.descriptor_match = display_match && touch_match && error == KMYC_CONTROLLER_TLV_END;
    if (!s_info.descriptor_match) ESP_LOGW(TAG, "Descriptor mismatch/malformed; using compiled preset");
}

esp_err_t kmyc_bridge_brightness(uint8_t duty)
{
    esp_err_t error = apply(0,0,0,0,true,duty);
    if (error == ESP_OK) s_info.brightness = duty;
    return error;
}

static esp_err_t touch_reset_sequence(void)
{
    /* Host-specific GT911 0x5D selection; never encoded inside MCU firmware. */
    esp_err_t error = apply(6,0,6,0x14,false,0);
    if (error != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(10)); /* INT low >100us before rising RST. */
    if ((error = apply(2,2,0,0,false,0)) != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(5));
    if ((error = apply(0,0,4,0,false,0)) != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(195)); /* total 200ms after RST high */
    return ESP_OK; /* TP_RST remains PP high; TP_INT input/no-pull. */
}

esp_err_t kmyc_adapter_touch_reset(void)
{
    esp_err_t error = touch_reset_sequence();
    if (error == ESP_OK) error = kmyc_touch_reprobe();
    return record(error, "touch reset/reprobe");
}

esp_err_t kmyc_bridge_prepare(void)
{
    i2c_master_bus_handle_t bus;
    esp_err_t error = kmyc_board_acquire_i2c(&bus);
    if (error != ESP_OK) return record(error, "shared I2C");
    s_info.completed_stages |= 1;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x2c,
        .scl_speed_hz = 400000, .scl_wait_us = 2000,
    };
    if ((error = i2c_master_bus_add_device(bus, &config, &s_device)) != ESP_OK) return error;
    const kmyc_controller_transport_t transport = {
        .context = s_device, .read = transport_read, .write = transport_write,
    };
    kmyc_controller_init(&s_client, &transport);
    kmyc_controller_info_t info;
    int result = kmyc_controller_probe(&s_client, &info);
    s_info.controller_present = result == 0 || result == KMYC_CONTROLLER_ERR_UNSUPPORTED;
    s_info.controller_compatible = !result && kmyc_controller_is_supported(&info, 7);
    s_info.controller_online = s_info.controller_compatible;
    if (!s_info.controller_compatible) {
        snprintf(s_info.system, sizeof(s_info.system), "Controller unavailable/incompatible (%d).\nDegraded: DCS reset + direct GT911; controller controls disabled.", result);
        snprintf(s_info.lifecycle, sizeof(s_info.lifecycle), "Fallback: no controller control frames");
        ESP_LOGW(TAG, "%s", s_info.system);
        return ESP_OK; /* Incompatible controller receives no control writes. */
    }
    s_info.completed_stages |= 2;
    read_descriptor();
    if ((error = kmyc_bridge_brightness(0)) != ESP_OK) return error;
    s_info.completed_stages |= 4;
    if ((error = apply(1,0,1,1,false,0)) != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(20));
    if ((error = apply(1,1,0,0,false,0)) != ESP_OK) return error;
    if ((error = apply(0,0,1,0,false,0)) != ESP_OK) return error;
    vTaskDelay(pdMS_TO_TICKS(120));
    s_info.completed_stages |= 8;
    if ((error = touch_reset_sequence()) != ESP_OK) return error;
    s_info.completed_stages |= 16;
    kmyc_adapter_refresh();
    return record(0, "controller/LCD/GT911 prepared");
}

esp_err_t kmyc_bridge_touch_sleep(void)
{
    esp_err_t error = apply(4, 0, 4, 0x10, false, 0);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Touch sleep INT-low failed: %s", esp_err_to_name(error));
        return error;
    }
    /* At least 5ms before the command; one extra tick covers tick-phase loss. */
    vTaskDelay(pdMS_TO_TICKS(5) + 1);
    error = kmyc_touch_enter_sleep();
    esp_err_t release = apply(0, 0, 4, 0, false, 0);
    ESP_LOGI(TAG, "Touch sleep command=%s INT release=%s asleep=%u",
             esp_err_to_name(error), esp_err_to_name(release), kmyc_touch_get_info()->asleep);
    return record(error != ESP_OK ? error : release, "touch sleep");
}

esp_err_t kmyc_bridge_touch_wake(void)
{
    esp_err_t error = apply(4, 4, 4, 0x10, false, 0);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Touch wake INT-high failed: %s", esp_err_to_name(error));
        return error;
    }
    vTaskDelay(pdMS_TO_TICKS(3));
    error = apply(0, 0, 4, 0, false, 0);
    ESP_LOGI(TAG, "Touch wake high-3ms pulse complete; INT release=%s", esp_err_to_name(error));
    if (error != ESP_OK) {
        return record(error, "touch wake INT release");
    }
    kmyc_touch_notify_wake_signal_complete();
    /* Readiness margin after releasing INT, not an extra pin pulse or a
     * datasheet-mandated delay. A failed identity read must not latch sleep. */
    vTaskDelay(pdMS_TO_TICKS(5));
    for (unsigned attempt = 1; attempt <= 2; attempt++) {
        error = kmyc_touch_reprobe();
        ESP_LOGI(TAG, "Touch wake identity %u/2: %s", attempt, esp_err_to_name(error));
        if (error == ESP_OK || attempt == 2) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    const kmyc_touch_info_t *touch = kmyc_touch_get_info();
    ESP_LOGI(TAG, "Touch wake final: asleep=%u available=%u error=%s",
             touch->asleep, touch->available, esp_err_to_name(touch->last_error));
    return record(error, "touch wake/reprobe");
}

esp_err_t kmyc_adapter_clear_diagnostics(void)
{
    if (!kmyc_bridge_has_control()) return ESP_ERR_NOT_SUPPORTED;
    if (xSemaphoreTake(s_lock,pdMS_TO_TICKS(500)) != pdTRUE) return ESP_ERR_TIMEOUT;
    int error = reconcile(kmyc_controller_clear_diagnostics(&s_client,NULL));
    xSemaphoreGive(s_lock);
    return record(error, "clear diagnostics");
}

esp_err_t kmyc_bridge_fail_safe(void)
{
    /* One bounded submission, no loop: PWM0 + LCD reset asserted, INT released. */
    return apply(1,0,5,1,true,0);
}
