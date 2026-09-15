#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/mipi_dsi_host_ll.h"

#include "kmyc_board.h"
#include "kmyc_display.h"
#include "kmyc_panel_product.h"
#include "kmyc_p4_pattern.h"
#include "wiring.h"

static const char *TAG = KMYC_ADAPTER_LOG_TAG;
static esp_lcd_panel_handle_t s_panel;
static bool s_internal_bist;

static esp_err_t init_dsi_panel(esp_lcd_panel_handle_t *panel_out, bool internal_bist)
{
    ESP_RETURN_ON_FALSE(panel_out, ESP_ERR_INVALID_ARG, TAG, "panel_out is null");

    ESP_RETURN_ON_ERROR(kmyc_board_power_display(), TAG, "failed to power display");

    ESP_LOGI(TAG, "Creating %d-lane DSI bus at %d Mbps/lane",
             KMYC_ADAPTER_DSI_LANE_NUM, KMYC_ADAPTER_LANE_BITRATE_MBPS);
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    const esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = KMYC_ADAPTER_DSI_LANE_NUM,
        .lane_bit_rate_mbps = KMYC_ADAPTER_LANE_BITRATE_MBPS,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus), TAG,
                        "failed to create DSI bus");

    ESP_LOGI(TAG, "Creating 8-bit MIPI DBI command channel");
    esp_lcd_panel_io_handle_t dbi_io = NULL;
    const esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io), TAG,
                        "failed to create DBI command channel");

    /* This panel does not return command acknowledgements on the adapter link. */
    ESP_LOGI(TAG, "Disabling DSI command acknowledgements for write-only panel link");
    mipi_dsi_host_ll_enable_cmd_ack(MIPI_DSI_LL_GET_HOST(0), KMYC_ADAPTER_COMMAND_ACK);

    const esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = KMYC_PANEL_DPI_CLOCK_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .num_fbs = 1,
        .video_timing = {
            .h_size = KMYC_PANEL_H_RES,
            .v_size = KMYC_PANEL_V_RES,
            .hsync_back_porch = KMYC_PANEL_HBP,
            .hsync_pulse_width = KMYC_PANEL_HSYNC,
            .hsync_front_porch = KMYC_PANEL_HFP,
            .vsync_back_porch = KMYC_PANEL_VBP,
            .vsync_pulse_width = KMYC_PANEL_VSYNC,
            .vsync_front_porch = KMYC_PANEL_VFP,
        },
    };

    ESP_LOGI(TAG, "Creating RGB888 DPI panel");
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_dpi(dsi_bus, &dpi_config, &panel), TAG,
                        "failed to create DPI panel");

    ESP_LOGI(TAG, "Disabling DPI frame acknowledgements for write-only panel link");
    mipi_dsi_host_ll_dpi_enable_frame_ack(MIPI_DSI_LL_GET_HOST(0), KMYC_ADAPTER_FRAME_ACK);

    ESP_LOGI(TAG, "Software-resetting panel (%s)", KMYC_ADAPTER_RESET_DESCRIPTION);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(dbi_io, LCD_CMD_SWRESET, NULL, 0), TAG,
                        "panel software reset failed");
    /* Required when the MCU restarts while the panel is already in Sleep Out mode. */
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "Initializing selected panel product without a DSI ID read");
    ESP_RETURN_ON_ERROR(kmyc_panel_product_initialize(
                            dbi_io, KMYC_ADAPTER_DSI_LANE_NUM), TAG,
                        "panel vendor initialization failed");

    if (internal_bist) {
        ESP_LOGW(TAG, "Diagnostic build: enabling panel-internal BIST; DPI video stays off");
        ESP_RETURN_ON_ERROR(kmyc_panel_product_enable_bist(dbi_io), TAG,
                            "failed to enable panel BIST");
    } else {
        ESP_LOGI(TAG, "Starting DPI video engine");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "DPI panel initialization failed");
    }

    *panel_out = panel;
    return ESP_OK;
}

static const kmyc_display_info_t s_info = {
    .model = KMYC_PANEL_MODEL,
    .width = KMYC_PANEL_H_RES,
    .height = KMYC_PANEL_V_RES,
    .active_lanes = KMYC_ADAPTER_DSI_LANE_NUM,
    .nominal_refresh_hz =
        (KMYC_PANEL_DPI_CLOCK_MHZ * 1000000.0) /
        ((KMYC_PANEL_H_RES + KMYC_PANEL_HSYNC + KMYC_PANEL_HBP + KMYC_PANEL_HFP) *
         (KMYC_PANEL_V_RES + KMYC_PANEL_VSYNC + KMYC_PANEL_VBP + KMYC_PANEL_VFP)),
};

const kmyc_display_info_t *kmyc_display_get_info(void)
{
    return &s_info;
}

esp_err_t kmyc_display_start(bool internal_bist)
{
    if (s_panel != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s_internal_bist = internal_bist;
    return init_dsi_panel(&s_panel, internal_bist);
}

esp_err_t kmyc_display_set_test_pattern(bool horizontal)
{
    if (s_panel == NULL || s_internal_bist) {
        return ESP_ERR_INVALID_STATE;
    }
    return kmyc_p4_set_test_pattern(s_panel, horizontal);
}

esp_err_t kmyc_display_draw_rgb888(int x_start, int y_start, int x_end, int y_end,
                                   const uint8_t *pixels)
{
    if (s_panel == NULL || s_internal_bist || pixels == NULL ||
        x_start < 0 || y_start < 0 || x_end <= x_start || y_end <= y_start ||
        x_end > s_info.width || y_end > s_info.height) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, pixels);
}
