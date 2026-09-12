#pragma once

#include <stddef.h>
#include "esp_err.h"
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CC10128007_31C_H_RES              800
#define CC10128007_31C_V_RES              1280
#define CC10128007_31C_HSYNC              8
#define CC10128007_31C_HBP                48
#define CC10128007_31C_HFP                52
#define CC10128007_31C_VSYNC              6
#define CC10128007_31C_VBP                16
#define CC10128007_31C_VFP                15
#define CC10128007_31C_DPI_CLOCK_MHZ      71
#define CC10128007_31C_DSI_LANE_NUM       2
#define CC10128007_31C_LANE_BITRATE_MBPS  800

const ili9881c_lcd_init_cmd_t *cc10128007_31c_get_init_commands(size_t *command_count);
esp_err_t cc10128007_31c_initialize(esp_lcd_panel_io_handle_t io);
esp_err_t cc10128007_31c_enable_bist(esp_lcd_panel_io_handle_t io);

#ifdef __cplusplus
}
#endif
