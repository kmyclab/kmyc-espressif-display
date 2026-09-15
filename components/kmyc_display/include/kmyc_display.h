#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Diagnostic API: one display and one owning app task.
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
/* Draw a compact, row-major RGB888 rectangle. The input buffer has no stride. */
esp_err_t kmyc_display_draw_rgb888(int x_start, int y_start, int x_end, int y_end,
                                   const uint8_t *pixels);
