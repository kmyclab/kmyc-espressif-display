#include "esp_lcd_mipi_dsi.h"
#include "kmyc_p4_pattern.h"

esp_err_t kmyc_p4_set_test_pattern(esp_lcd_panel_handle_t panel, bool horizontal)
{
    const mipi_dsi_pattern_type_t pattern = horizontal
        ? MIPI_DSI_PATTERN_BAR_HORIZONTAL
        : MIPI_DSI_PATTERN_BAR_VERTICAL;
    return esp_lcd_dpi_panel_set_pattern(panel, pattern);
}
