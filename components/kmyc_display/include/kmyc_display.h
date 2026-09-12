#pragma once

#include <stdbool.h>
#include "esp_err.h"

/* Diagnostic API: one display, one owning app task, no framebuffer ownership.
 * These calls are synchronous and must not be called from an ISR.
 * On startup failure the app must reboot; retry/teardown is not implemented.
 * This is not yet a general LVGL/USB framebuffer API.
 */
typedef struct {
    const char *model;
    int width;
    int height;
    int active_lanes;
    double nominal_refresh_hz;
} kmyc_display_info_t;

/* Static immutable storage, valid for the lifetime of the firmware. */
const kmyc_display_info_t *kmyc_display_get_info(void);
esp_err_t kmyc_display_start(bool internal_bist);
/* Valid only after successful startup with internal_bist == false. */
esp_err_t kmyc_display_set_test_pattern(bool horizontal);
