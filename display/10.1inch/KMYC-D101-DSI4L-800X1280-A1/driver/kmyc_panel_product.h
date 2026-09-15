#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"

#define KMYC_PANEL_MODEL              "KMYC-D101-DSI4L-800X1280-A1"
#define KMYC_PANEL_H_RES              800
#define KMYC_PANEL_V_RES              1280
#define KMYC_PANEL_HSYNC              8
#define KMYC_PANEL_HBP                48
#define KMYC_PANEL_HFP                52
#define KMYC_PANEL_VSYNC              6
#define KMYC_PANEL_VBP                16
#define KMYC_PANEL_VFP                15
#define KMYC_PANEL_DPI_CLOCK_MHZ      71

esp_err_t kmyc_panel_product_initialize(esp_lcd_panel_io_handle_t io,
                                        int active_lanes);
esp_err_t kmyc_panel_product_enable_bist(esp_lcd_panel_io_handle_t io);
