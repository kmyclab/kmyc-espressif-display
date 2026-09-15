#pragma once

#include "esp_err.h"
#include "kmyc_display.h"
#include "kmyc_touch.h"

esp_err_t touch_view_start(const kmyc_display_info_t *display);
esp_err_t touch_view_update(const kmyc_touch_report_t *report);
