#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

esp_err_t kmyc_p4_set_test_pattern(esp_lcd_panel_handle_t panel, bool horizontal);
