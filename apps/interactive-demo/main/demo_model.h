#pragma once

#include "kmyc_adapter.h"
#include "kmyc_touch.h"

/* Value-only snapshot: no device handles or LVGL objects cross task ownership. */
typedef struct {
    kmyc_adapter_info_t adapter;
    kmyc_touch_info_t touch;
    kmyc_touch_report_t report;
    uint32_t report_sequence;
    bool ready, boot_failed, asleep;
    esp_err_t error;
    char history[6][80];
} demo_model_t;
