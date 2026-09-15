#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"

#define KMYC_PANEL_MODEL              "KMYC-D070-DSI4L-1024X600-A1"
#define KMYC_PANEL_H_RES              1024
#define KMYC_PANEL_V_RES              600
#define KMYC_PANEL_HSYNC              24
#define KMYC_PANEL_HBP                136
#define KMYC_PANEL_HFP                160
#define KMYC_PANEL_VSYNC              2
#define KMYC_PANEL_VBP                21
#define KMYC_PANEL_VFP                12
#define KMYC_PANEL_DPI_CLOCK_MHZ      51

esp_err_t kmyc_panel_product_initialize(esp_lcd_panel_io_handle_t io,
                                        int active_lanes);
esp_err_t kmyc_panel_product_enable_bist(esp_lcd_panel_io_handle_t io);
