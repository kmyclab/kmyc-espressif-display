#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define KMYC_TOUCH_MAX_POINTS 5

typedef struct {
    uint8_t id;
    uint16_t x;
    uint16_t y;
    uint16_t strength;
} kmyc_touch_point_t;

typedef struct {
    uint8_t count;
    kmyc_touch_point_t points[KMYC_TOUCH_MAX_POINTS];
} kmyc_touch_report_t;

esp_err_t kmyc_touch_start(void);
esp_err_t kmyc_touch_read(kmyc_touch_report_t *report, bool *updated);

typedef struct {
    char product_id[5];
    uint16_t firmware_version;
    uint8_t address;
    bool available;
    bool asleep;
    esp_err_t last_error;
} kmyc_touch_info_t;

const kmyc_touch_info_t *kmyc_touch_get_info(void);
/* One app task owns read/lifecycle calls. No configuration firmware writes. */
esp_err_t kmyc_touch_reprobe(void);
esp_err_t kmyc_touch_enter_sleep(void); /* caller owns INT sequencing */
/* Host has completed the wake signal and released INT. Allow bus access again;
 * this is not proof of device readiness or identity and performs no I2C access. */
void kmyc_touch_notify_wake_signal_complete(void);
