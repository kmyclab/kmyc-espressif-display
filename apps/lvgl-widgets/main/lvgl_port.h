// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "esp_err.h"

/* Runs in the caller's task for the application lifetime; returns on startup error. */
esp_err_t lvgl_port_run(void);
