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
