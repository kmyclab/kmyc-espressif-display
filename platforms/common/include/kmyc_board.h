#pragma once

#include "esp_err.h"

/* Board-owned power resource; acquired once for the lifetime of the app. */
esp_err_t kmyc_board_power_display(void);
