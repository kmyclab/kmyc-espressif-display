// SPDX-License-Identifier: GPL-2.0-only

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "kmyc_panel_product.h"

typedef struct {
    uint8_t command;
    const uint8_t *data;
    size_t data_bytes;
    unsigned int delay_ms;
    bool lane_select;
} panel_command_t;

#define CMD0_DELAY(command_value, delay_value) \
    { (command_value), NULL, 0, (delay_value), false }
#define CMD1(command_value, value) \
    { (command_value), (const uint8_t[]){(value)}, 1, 0, false }
#define CMD2(command_value, value0, value1) \
    { (command_value), (const uint8_t[]){(value0), (value1)}, 2, 0, false }
#define CMD4(command_value, value0, value1, value2, value3) \
    { (command_value), (const uint8_t[]){(value0), (value1), (value2), (value3)}, 4, 0, false }
#define CMD11(command_value, ...) \
    { (command_value), (const uint8_t[]){__VA_ARGS__}, 11, 0, false }
#define CMD14(command_value, ...) \
    { (command_value), (const uint8_t[]){__VA_ARGS__}, 14, 0, false }
#define LANE_SELECT(command_value) \
    { (command_value), NULL, 1, 0, true }

/* Ported from KMYC's public Raspberry Pi JD9165BA driver. */
static const panel_command_t s_init_commands[] = {
    CMD1(0x30, 0x00),
    CMD4(0xF7, 0x49, 0x61, 0x02, 0x00),
    CMD1(0x30, 0x01),
    CMD1(0x04, 0x0C),
    CMD1(0x05, 0x08),
    LANE_SELECT(0x0B),
    CMD1(0x23, 0x38),
    CMD1(0x28, 0x18),
    CMD1(0x29, 0x29),
    CMD1(0x2A, 0x01),
    CMD1(0x2B, 0x29),
    CMD1(0x2C, 0x01),
    CMD1(0x30, 0x02),
    CMD1(0x00, 0x05),
    CMD1(0x01, 0x22),
    CMD1(0x02, 0x08),
    CMD1(0x03, 0x12),
    CMD1(0x04, 0x16),
    CMD1(0x05, 0x64),
    CMD1(0x06, 0x00),
    CMD1(0x07, 0x00),
    CMD1(0x08, 0x78),
    CMD1(0x09, 0x00),
    CMD1(0x0A, 0x04),
    CMD11(0x0B, 0x16, 0x17, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x07, 0x07, 0x09),
    CMD11(0x0C, 0x09, 0x1E, 0x1E, 0x1C, 0x1C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    CMD11(0x0D, 0x0A, 0x05, 0x0B, 0x0D, 0x0D, 0x0D, 0x11, 0x10, 0x06, 0x06, 0x08),
    CMD11(0x0E, 0x08, 0x1F, 0x1F, 0x1D, 0x1D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    CMD11(0x0F, 0x0A, 0x05, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1D, 0x1D, 0x1F),
    CMD11(0x10, 0x1F, 0x08, 0x08, 0x06, 0x06, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    CMD11(0x11, 0x16, 0x17, 0x0D, 0x0B, 0x0D, 0x0D, 0x11, 0x10, 0x1C, 0x1C, 0x1E),
    CMD11(0x12, 0x1E, 0x09, 0x09, 0x07, 0x07, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D),
    CMD4(0x13, 0x00, 0x00, 0x00, 0x00),
    CMD4(0x14, 0x00, 0x00, 0x41, 0x41),
    CMD4(0x15, 0x00, 0x00, 0x00, 0x00),
    CMD1(0x17, 0x00),
    CMD1(0x18, 0x85),
    CMD2(0x19, 0x06, 0x09),
    CMD2(0x1A, 0x05, 0x08),
    CMD2(0x1B, 0x0A, 0x04),
    CMD1(0x26, 0x00),
    CMD1(0x27, 0x00),
    CMD1(0x30, 0x06),
    CMD14(0x12, 0x3F, 0x26, 0x27, 0x35, 0x2D, 0x34, 0x3F, 0x3F, 0x3F, 0x35, 0x2A, 0x20, 0x16, 0x08),
    CMD14(0x13, 0x3F, 0x26, 0x28, 0x35, 0x27, 0x29, 0x29, 0x2F, 0x35, 0x2F, 0x26, 0x20, 0x16, 0x08),
    CMD1(0x30, 0x0A),
    CMD1(0x02, 0x4F),
    CMD1(0x0B, 0x40),
    CMD1(0x30, 0x0D),
    CMD1(0x0D, 0x04),
    CMD1(0x10, 0x0C),
    CMD1(0x11, 0x0C),
    CMD1(0x12, 0x0C),
    CMD1(0x13, 0x0C),
    CMD1(0x30, 0x00),
    CMD0_DELAY(0x11, 120),
    CMD0_DELAY(0x29, 20),
};

static const char *TAG = "hyy7bips27a";

static esp_err_t lane_select_value(int active_lanes, uint8_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (active_lanes) {
    case 2:
        *value = 0x11;
        return ESP_OK;
    case 3:
        *value = 0x12;
        return ESP_OK;
    case 4:
        *value = 0x13;
        return ESP_OK;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t kmyc_panel_product_initialize(esp_lcd_panel_io_handle_t io,
                                        int active_lanes)
{
    if (io == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t lane_value = 0;
    esp_err_t err = lane_select_value(active_lanes, &lane_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "JD9165BA does not support %d active lanes", active_lanes);
        return err;
    }

    const size_t command_count = sizeof(s_init_commands) / sizeof(s_init_commands[0]);
    ESP_LOGI(TAG, "Sending %u JD9165BA commands for %d active lanes",
             (unsigned)command_count, active_lanes);
    for (size_t index = 0; index < command_count; index++) {
        const panel_command_t *entry = &s_init_commands[index];
        const uint8_t *data = entry->lane_select ? &lane_value : entry->data;
        err = esp_lcd_panel_io_tx_param(io, entry->command, data, entry->data_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Command %u/%u (0x%02X) failed: %s",
                     (unsigned)(index + 1), (unsigned)command_count,
                     entry->command, esp_err_to_name(err));
            return err;
        }
        if (entry->lane_select && active_lanes != 4) {
            const uint8_t software_lane_select = 0x04;
            err = esp_lcd_panel_io_tx_param(io, 0x20,
                                            &software_lane_select,
                                            sizeof(software_lane_select));
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Software lane-select command failed: %s",
                         esp_err_to_name(err));
                return err;
            }
            ESP_LOGI(TAG, "Software lane selection enabled");
        }
        if (entry->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(entry->delay_ms));
        }
    }

    ESP_LOGI(TAG, "JD9165BA initialization complete");
    return ESP_OK;
}

esp_err_t kmyc_panel_product_enable_bist(esp_lcd_panel_io_handle_t io)
{
    (void)io;
    ESP_LOGE(TAG, "No verified JD9165BA internal BIST command is published");
    return ESP_ERR_NOT_SUPPORTED;
}
