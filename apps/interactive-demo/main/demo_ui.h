#pragma once

#include "esp_err.h"

/* Start after demo_service_start(). Only this task owns LVGL objects. */
esp_err_t demo_ui_start(void);
