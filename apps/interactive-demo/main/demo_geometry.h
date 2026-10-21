#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* RGB888 partial rendering: size_t allocation and LVGL uint32_t size must both
 * fit. Dimensions come from the selected display, not touch-native axes. */
static inline bool demo_buffer_size(int width, int height, size_t *bytes)
{
    if (!bytes || width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / 40 / 3 ||
        (size_t)width > UINT32_MAX / 40 / 3) {
        return false;
    }
    *bytes = (size_t)width * 40 * 3;
    return true;
}

static inline bool demo_point_is_edge(int x, int y, int width, int height)
{
    const int margin = 20;
    return x < margin || y < margin || x >= width - margin || y >= height - margin;
}
