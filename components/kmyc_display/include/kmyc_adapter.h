#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool controller_present, controller_compatible, controller_online, descriptor_match;
    uint8_t consecutive_status_failures;
    uint32_t completed_stages;
    uint8_t brightness;
    int last_error;
    char system[512];
    char descriptor[1024];
    char lifecycle[192];
} kmyc_adapter_info_t;

/* These lifecycle APIs are implemented by the bridge-v1.2 recipe only.
 * One application task owns display, touch, adapter and LVGL operations. */
const kmyc_adapter_info_t *kmyc_adapter_get_info(void);
esp_err_t kmyc_adapter_refresh(void);
esp_err_t kmyc_adapter_clear_diagnostics(void);
esp_err_t kmyc_adapter_touch_reset(void);
