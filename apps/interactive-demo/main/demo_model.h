#pragma once

#include "kmyc_adapter.h"
#include "kmyc_touch.h"

/* Copied values plus immutable identity strings; no device handles or LVGL
 * objects cross task ownership. String lifetimes equal firmware lifetime. */
typedef struct {
    kmyc_adapter_info_t adapter;
    kmyc_touch_info_t touch;
    kmyc_touch_report_t report;
    uint32_t report_sequence;
    bool ready, boot_failed, asleep, touch_enabled;
    esp_err_t error;
    char history[6][80];
} demo_model_t;
