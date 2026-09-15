#pragma once

/* The P4-Pico exposes DSI and shared I2C on its 22-pin connector, but no LCD
 * reset or backlight GPIO. The physical display adapter model is not registered.
 */
#define KMYC_ADAPTER_COMMAND_ACK false
#define KMYC_ADAPTER_FRAME_ACK false
#define KMYC_ADAPTER_DSI_LANE_NUM 2
#define KMYC_ADAPTER_LANE_BITRATE_MBPS 1000
#define KMYC_ADAPTER_LOG_TAG "waveshare_dsi"
#define KMYC_ADAPTER_RESET_DESCRIPTION \
    "the P4-Pico DSI connector exposes no panel-reset GPIO"
