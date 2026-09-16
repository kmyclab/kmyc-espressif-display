#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* One display, one lifecycle service owner. After startup, one separate drawing
 * task may copy pixels; no other callers may mutate lifecycle/panel state.
 * Calls are task-only. Flush completion callback can run in ISR/task context.
 * Startup retry/teardown is not implemented. This is not a multi-display API.
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
/* Bridge-enabled recipe only; legacy recipes return NOT_SUPPORTED. */
esp_err_t kmyc_display_set_brightness(uint8_t percent); /* 0..100 -> round(percent*255/100) */
typedef bool (*kmyc_display_flush_done_cb_t)(void *context);
/* Callback can be ISR/task context; never call LVGL there. Return task-woken flag. */
esp_err_t kmyc_display_set_flush_done_callback(kmyc_display_flush_done_cb_t callback, void *context);
esp_err_t kmyc_display_sleep(void);
esp_err_t kmyc_display_wake(void);
bool kmyc_display_is_asleep(void);
