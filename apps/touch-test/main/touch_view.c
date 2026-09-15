// SPDX-License-Identifier: GPL-2.0-only

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"

#include "touch_view.h"

#define BYTES_PER_PIXEL 3
#define BACKGROUND_STRIP_HEIGHT 8
#define GRID_SIZE 100
#define BORDER_WIDTH 6
#define TARGET_RADIUS 32
#define TARGET_HIT_RADIUS 72
#define CONTACT_OUTER_RADIUS 28
#define CONTACT_INNER_RADIUS 21
#define STATUS_CELL_SIZE 22
#define STATUS_CELL_GAP 8
#define STATUS_MARGIN_TOP 10

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb888_t;

typedef struct {
    int x_start;
    int y_start;
    int x_end;
    int y_end;
} rect_t;

static const char *TAG = "touch_view";
static const rgb888_t s_contact_colors[KMYC_TOUCH_MAX_POINTS] = {
    {255, 48, 48},
    {48, 224, 255},
    {255, 224, 48},
    {64, 255, 96},
    {224, 64, 255},
};

static const kmyc_display_info_t *s_display;
static uint8_t *s_pixels;
static size_t s_pixel_capacity;
static kmyc_touch_report_t s_previous_report;
static bool s_targets_hit[5];
static uint8_t s_maximum_contacts;

static int minimum(int left, int right)
{
    return left < right ? left : right;
}

static int maximum(int left, int right)
{
    return left > right ? left : right;
}

static size_t maximum_size(size_t left, size_t right)
{
    return left > right ? left : right;
}

static rgb888_t target_color(size_t index)
{
    return s_targets_hit[index] ? (rgb888_t){32, 255, 96} : (rgb888_t){255, 96, 32};
}

static void get_target(size_t index, int *x, int *y)
{
    const int inset = TARGET_HIT_RADIUS;
    const int right = s_display->width - 1 - inset;
    const int bottom = s_display->height - 1 - inset;
    const int positions[5][2] = {
        {inset, inset},
        {right, inset},
        {right, bottom},
        {inset, bottom},
        {s_display->width / 2, s_display->height / 2},
    };
    *x = positions[index][0];
    *y = positions[index][1];
}

static bool inside_circle(int x, int y, int center_x, int center_y, int radius)
{
    const int dx = x - center_x;
    const int dy = y - center_y;
    return dx * dx + dy * dy <= radius * radius;
}

static rgb888_t background_pixel(int x, int y)
{
    rgb888_t color;
    const bool right = x >= s_display->width / 2;
    const bool bottom = y >= s_display->height / 2;
    if (!right && !bottom) {
        color = (rgb888_t){28, 12, 16};
    } else if (right && !bottom) {
        color = (rgb888_t){10, 26, 18};
    } else if (right) {
        color = (rgb888_t){24, 24, 24};
    } else {
        color = (rgb888_t){10, 18, 32};
    }

    if ((x % GRID_SIZE) < 2 || (y % GRID_SIZE) < 2) {
        color = (rgb888_t){60, 64, 68};
    }

    if (y < BORDER_WIDTH) {
        color = (rgb888_t){255, 48, 48};
    } else if (x >= s_display->width - BORDER_WIDTH) {
        color = (rgb888_t){48, 255, 96};
    } else if (y >= s_display->height - BORDER_WIDTH) {
        color = (rgb888_t){48, 96, 255};
    } else if (x < BORDER_WIDTH) {
        color = (rgb888_t){255, 224, 48};
    }

    for (size_t index = 0; index < 5; index++) {
        int target_x;
        int target_y;
        get_target(index, &target_x, &target_y);
        const bool outer = inside_circle(x, y, target_x, target_y, TARGET_RADIUS);
        const bool inner = inside_circle(x, y, target_x, target_y, TARGET_RADIUS - 5);
        if (outer && !inner) {
            color = target_color(index);
        }
        if ((abs(x - target_x) <= 1 && abs(y - target_y) <= TARGET_RADIUS) ||
            (abs(y - target_y) <= 1 && abs(x - target_x) <= TARGET_RADIUS)) {
            color = target_color(index);
        }
    }

    const int status_width = KMYC_TOUCH_MAX_POINTS * STATUS_CELL_SIZE +
                             (KMYC_TOUCH_MAX_POINTS - 1) * STATUS_CELL_GAP;
    const int status_x = (s_display->width - status_width) / 2;
    for (uint8_t index = 0; index < KMYC_TOUCH_MAX_POINTS; index++) {
        const int cell_x = status_x + index * (STATUS_CELL_SIZE + STATUS_CELL_GAP);
        const bool in_cell = x >= cell_x && x < cell_x + STATUS_CELL_SIZE &&
                             y >= STATUS_MARGIN_TOP &&
                             y < STATUS_MARGIN_TOP + STATUS_CELL_SIZE;
        if (in_cell) {
            const bool border = x == cell_x || x == cell_x + STATUS_CELL_SIZE - 1 ||
                                y == STATUS_MARGIN_TOP ||
                                y == STATUS_MARGIN_TOP + STATUS_CELL_SIZE - 1;
            if (index < s_previous_report.count) {
                color = s_contact_colors[index];
            } else if (border && index < s_maximum_contacts) {
                color = (rgb888_t){32, 255, 96};
            } else if (border) {
                color = (rgb888_t){112, 112, 112};
            } else {
                color = (rgb888_t){8, 8, 8};
            }
        }
    }
    return color;
}

static rgb888_t composed_pixel(int x, int y, const kmyc_touch_report_t *report)
{
    rgb888_t color = background_pixel(x, y);
    for (uint8_t index = 0; index < report->count; index++) {
        const kmyc_touch_point_t *point = &report->points[index];
        if (!inside_circle(x, y, point->x, point->y, CONTACT_OUTER_RADIUS)) {
            continue;
        }
        const bool inner = inside_circle(x, y, point->x, point->y, CONTACT_INNER_RADIUS);
        color = inner ? s_contact_colors[point->id % KMYC_TOUCH_MAX_POINTS]
                      : (rgb888_t){255, 255, 255};
        if (abs(x - point->x) <= 1 || abs(y - point->y) <= 1) {
            color = (rgb888_t){0, 0, 0};
        }
    }
    return color;
}

static esp_err_t render_rect(rect_t rect, const kmyc_touch_report_t *report)
{
    rect.x_start = maximum(rect.x_start, 0);
    rect.y_start = maximum(rect.y_start, 0);
    rect.x_end = minimum(rect.x_end, s_display->width);
    rect.y_end = minimum(rect.y_end, s_display->height);
    if (rect.x_end <= rect.x_start || rect.y_end <= rect.y_start) {
        return ESP_OK;
    }

    const int width = rect.x_end - rect.x_start;
    const int height = rect.y_end - rect.y_start;
    const size_t required = (size_t)width * height * BYTES_PER_PIXEL;
    ESP_RETURN_ON_FALSE(required <= s_pixel_capacity, ESP_ERR_NO_MEM, TAG,
                        "render area exceeds scratch buffer");

    uint8_t *pixel = s_pixels;
    for (int y = rect.y_start; y < rect.y_end; y++) {
        for (int x = rect.x_start; x < rect.x_end; x++) {
            const rgb888_t color = composed_pixel(x, y, report);
            *pixel++ = color.red;
            *pixel++ = color.green;
            *pixel++ = color.blue;
        }
    }
    return kmyc_display_draw_rgb888(rect.x_start, rect.y_start, rect.x_end, rect.y_end,
                                    s_pixels);
}

static rect_t contact_rect(const kmyc_touch_point_t *point)
{
    return (rect_t){
        .x_start = point->x - CONTACT_OUTER_RADIUS - 2,
        .y_start = point->y - CONTACT_OUTER_RADIUS - 2,
        .x_end = point->x + CONTACT_OUTER_RADIUS + 3,
        .y_end = point->y + CONTACT_OUTER_RADIUS + 3,
    };
}

static esp_err_t render_status(const kmyc_touch_report_t *report)
{
    const int status_width = KMYC_TOUCH_MAX_POINTS * STATUS_CELL_SIZE +
                             (KMYC_TOUCH_MAX_POINTS - 1) * STATUS_CELL_GAP;
    const int status_x = (s_display->width - status_width) / 2;
    return render_rect((rect_t){status_x, STATUS_MARGIN_TOP,
                                status_x + status_width,
                                STATUS_MARGIN_TOP + STATUS_CELL_SIZE}, report);
}

esp_err_t touch_view_start(const kmyc_display_info_t *display)
{
    ESP_RETURN_ON_FALSE(display != NULL && display->width >= 320 && display->height >= 320,
                        ESP_ERR_INVALID_ARG, TAG, "display must be at least 320x320");
    ESP_RETURN_ON_FALSE(s_pixels == NULL, ESP_ERR_INVALID_STATE, TAG,
                        "touch view is already initialized");
    s_display = display;
    const size_t strip_size = (size_t)display->width * BACKGROUND_STRIP_HEIGHT *
                              BYTES_PER_PIXEL;
    const size_t contact_size = (CONTACT_OUTER_RADIUS * 2 + 5) *
                                (CONTACT_OUTER_RADIUS * 2 + 5) * BYTES_PER_PIXEL;
    const size_t target_size = (TARGET_RADIUS * 2 + 7) *
                               (TARGET_RADIUS * 2 + 7) * BYTES_PER_PIXEL;
    const size_t status_size = (KMYC_TOUCH_MAX_POINTS * STATUS_CELL_SIZE +
                                (KMYC_TOUCH_MAX_POINTS - 1) * STATUS_CELL_GAP) *
                               STATUS_CELL_SIZE * BYTES_PER_PIXEL;
    s_pixel_capacity = maximum_size(maximum_size(strip_size, contact_size),
                                    maximum_size(target_size, status_size));
    s_pixels = malloc(s_pixel_capacity);
    ESP_RETURN_ON_FALSE(s_pixels != NULL, ESP_ERR_NO_MEM, TAG,
                        "failed to allocate %u-byte scratch buffer",
                        (unsigned)s_pixel_capacity);

    memset(&s_previous_report, 0, sizeof(s_previous_report));
    memset(s_targets_hit, 0, sizeof(s_targets_hit));
    s_maximum_contacts = 0;
    for (int y = 0; y < display->height; y += BACKGROUND_STRIP_HEIGHT) {
        const int y_end = minimum(y + BACKGROUND_STRIP_HEIGHT, display->height);
        ESP_RETURN_ON_ERROR(render_rect((rect_t){0, y, display->width, y_end},
                                        &s_previous_report), TAG,
                            "failed to draw diagnostic background");
    }
    ESP_LOGI(TAG, "Ready: touch five targets and press up to %d points together",
             KMYC_TOUCH_MAX_POINTS);
    return ESP_OK;
}

esp_err_t touch_view_update(const kmyc_touch_report_t *report)
{
    ESP_RETURN_ON_FALSE(s_pixels != NULL && report != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "touch view is not initialized");
    ESP_RETURN_ON_FALSE(report->count <= KMYC_TOUCH_MAX_POINTS, ESP_ERR_INVALID_ARG, TAG,
                        "invalid contact count");

    bool targets_changed[5] = {false};
    for (uint8_t point_index = 0; point_index < report->count; point_index++) {
        const kmyc_touch_point_t *point = &report->points[point_index];
        for (size_t target_index = 0; target_index < 5; target_index++) {
            int target_x;
            int target_y;
            get_target(target_index, &target_x, &target_y);
            if (!s_targets_hit[target_index] &&
                inside_circle(point->x, point->y, target_x, target_y,
                              TARGET_HIT_RADIUS)) {
                s_targets_hit[target_index] = true;
                targets_changed[target_index] = true;
                ESP_LOGI(TAG, "target %u reached", (unsigned)target_index + 1);
            }
        }
    }
    if (report->count > s_maximum_contacts) {
        s_maximum_contacts = report->count;
        ESP_LOGI(TAG, "maximum simultaneous contacts: %u/%u",
                 s_maximum_contacts, KMYC_TOUCH_MAX_POINTS);
    }

    const kmyc_touch_report_t next_report = *report;
    for (uint8_t index = 0; index < s_previous_report.count; index++) {
        ESP_RETURN_ON_ERROR(render_rect(contact_rect(&s_previous_report.points[index]),
                                        &next_report), TAG,
                            "failed to erase previous contact");
    }
    for (uint8_t index = 0; index < next_report.count; index++) {
        ESP_RETURN_ON_ERROR(render_rect(contact_rect(&next_report.points[index]),
                                        &next_report), TAG,
                            "failed to draw contact");
    }
    s_previous_report = next_report;

    for (size_t index = 0; index < 5; index++) {
        if (targets_changed[index]) {
            int target_x;
            int target_y;
            get_target(index, &target_x, &target_y);
            const int radius = TARGET_RADIUS + 3;
            ESP_RETURN_ON_ERROR(render_rect((rect_t){target_x - radius,
                                                     target_y - radius,
                                                     target_x + radius + 1,
                                                     target_y + radius + 1},
                                            &s_previous_report), TAG,
                                "failed to update target");
        }
    }
    return render_status(&s_previous_report);
}
