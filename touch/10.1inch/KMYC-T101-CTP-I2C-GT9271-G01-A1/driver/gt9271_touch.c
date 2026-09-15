// SPDX-License-Identifier: GPL-2.0-only

#include <ctype.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "kmyc_touch.h"
#include "touch_wiring.h"

#define GT9271_PRODUCT_ID_REG 0x8140
#define GT9271_REPORT_STATUS_REG 0x814E
#define GT9271_FIRST_POINT_REG 0x814F
#define GT9271_POINT_BYTES 8

static const char *TAG = "gt9271";
static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_device;

static esp_err_t read_register(uint16_t reg, uint8_t *data, size_t size)
{
    const uint8_t address[] = {(uint8_t)(reg >> 8), (uint8_t)reg};
    return i2c_master_transmit_receive(s_device, address, sizeof(address), data, size,
                                       KMYC_TOUCH_I2C_TIMEOUT_MS);
}

static esp_err_t write_u8(uint16_t reg, uint8_t value)
{
    const uint8_t data[] = {(uint8_t)(reg >> 8), (uint8_t)reg, value};
    return i2c_master_transmit(s_device, data, sizeof(data), KMYC_TOUCH_I2C_TIMEOUT_MS);
}

static void transform_point(uint16_t raw_x, uint16_t raw_y, uint16_t *x, uint16_t *y)
{
    uint16_t mapped_x = raw_x;
    uint16_t mapped_y = raw_y;
#if KMYC_TOUCH_SWAP_XY
    const uint16_t temporary = mapped_x;
    mapped_x = mapped_y;
    mapped_y = temporary;
#endif
    if (mapped_x >= KMYC_TOUCH_OUTPUT_WIDTH) {
        mapped_x = KMYC_TOUCH_OUTPUT_WIDTH - 1;
    }
    if (mapped_y >= KMYC_TOUCH_OUTPUT_HEIGHT) {
        mapped_y = KMYC_TOUCH_OUTPUT_HEIGHT - 1;
    }
#if KMYC_TOUCH_MIRROR_X
    mapped_x = KMYC_TOUCH_OUTPUT_WIDTH - 1 - mapped_x;
#endif
#if KMYC_TOUCH_MIRROR_Y
    mapped_y = KMYC_TOUCH_OUTPUT_HEIGHT - 1 - mapped_y;
#endif
    *x = mapped_x;
    *y = mapped_y;
}

esp_err_t kmyc_touch_start(void)
{
    ESP_RETURN_ON_FALSE(s_bus == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "touch is already initialized");
    ESP_LOGI(TAG, "Starting polling touch on I2C%d: SCL GPIO%d, SDA GPIO%d",
             KMYC_TOUCH_I2C_PORT, KMYC_TOUCH_I2C_SCL_GPIO, KMYC_TOUCH_I2C_SDA_GPIO);
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = KMYC_TOUCH_I2C_PORT,
        .sda_io_num = KMYC_TOUCH_I2C_SDA_GPIO,
        .scl_io_num = KMYC_TOUCH_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_bus), TAG,
                        "failed to create shared I2C bus");

    vTaskDelay(pdMS_TO_TICKS(KMYC_TOUCH_STARTUP_DELAY_MS));
    uint8_t device_address = 0;
    const uint8_t candidates[] = {KMYC_TOUCH_PRIMARY_ADDRESS, KMYC_TOUCH_BACKUP_ADDRESS};
    for (size_t index = 0; index < sizeof(candidates); index++) {
        if (i2c_master_probe(s_bus, candidates[index], KMYC_TOUCH_I2C_TIMEOUT_MS) == ESP_OK) {
            device_address = candidates[index];
            break;
        }
    }
    ESP_RETURN_ON_FALSE(device_address != 0, ESP_ERR_NOT_FOUND, TAG,
                        "GT9271 not found at 0x%02X or 0x%02X",
                        KMYC_TOUCH_PRIMARY_ADDRESS, KMYC_TOUCH_BACKUP_ADDRESS);

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_address,
        .scl_speed_hz = KMYC_TOUCH_I2C_SPEED_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_bus, &device_config, &s_device), TAG,
                        "failed to add GT9271 I2C device");

    uint8_t identity[6] = {0};
    ESP_RETURN_ON_ERROR(read_register(GT9271_PRODUCT_ID_REG, identity, sizeof(identity)), TAG,
                        "failed to read GT9271 identity");
    char product_id[5] = {0};
    for (size_t index = 0; index < sizeof(product_id) - 1; index++) {
        product_id[index] = isprint(identity[index]) ? (char)identity[index] : '?';
    }
    const uint16_t firmware_version = identity[4] | ((uint16_t)identity[5] << 8);
    ESP_RETURN_ON_FALSE(strcmp(product_id, "9271") == 0, ESP_ERR_NOT_SUPPORTED, TAG,
                        "expected GT9271, found product ID '%s'", product_id);
    ESP_LOGI(TAG, "Detected GT%s at 0x%02X, firmware 0x%04X", product_id,
             device_address, firmware_version);
    ESP_LOGI(TAG, "Mapping native 1280x800 coordinates to %dx%d: swap XY, mirror X",
             KMYC_TOUCH_OUTPUT_WIDTH, KMYC_TOUCH_OUTPUT_HEIGHT);
    return ESP_OK;
}

esp_err_t kmyc_touch_read(kmyc_touch_report_t *report, bool *updated)
{
    ESP_RETURN_ON_FALSE(s_device != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "touch is not initialized");
    ESP_RETURN_ON_FALSE(report != NULL && updated != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "report and updated are required");
    memset(report, 0, sizeof(*report));
    *updated = false;

    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(read_register(GT9271_REPORT_STATUS_REG, &status, 1), TAG,
                        "failed to read touch status");
    if ((status & 0x80U) == 0) {
        return ESP_OK;
    }

    const uint8_t count = status & 0x0FU;
    if (count > KMYC_TOUCH_MAX_POINTS) {
        ESP_RETURN_ON_ERROR(write_u8(GT9271_REPORT_STATUS_REG, 0), TAG,
                            "failed to clear invalid touch report");
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t raw[KMYC_TOUCH_MAX_POINTS * GT9271_POINT_BYTES] = {0};
    if (count > 0) {
        ESP_RETURN_ON_ERROR(read_register(GT9271_FIRST_POINT_REG, raw,
                                          count * GT9271_POINT_BYTES), TAG,
                            "failed to read touch points");
    }
    for (uint8_t index = 0; index < count; index++) {
        const uint8_t *point = &raw[index * GT9271_POINT_BYTES];
        const uint16_t raw_x = point[1] | ((uint16_t)point[2] << 8);
        const uint16_t raw_y = point[3] | ((uint16_t)point[4] << 8);
        report->points[index].id = point[0] & 0x0FU;
        report->points[index].strength = point[5] | ((uint16_t)point[6] << 8);
        transform_point(raw_x, raw_y, &report->points[index].x, &report->points[index].y);
    }
    report->count = count;

    /* Clear 0x814E only after a ready report has been consumed. */
    ESP_RETURN_ON_ERROR(write_u8(GT9271_REPORT_STATUS_REG, 0), TAG,
                        "failed to acknowledge touch report");
    *updated = true;
    return ESP_OK;
}
