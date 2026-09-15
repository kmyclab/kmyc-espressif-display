#pragma once

/* The P4-Pico exposes two DSI data lanes and no LCD reset or backlight GPIO. */
#define KMYC_ADAPTER_COMMAND_ACK false
#define KMYC_ADAPTER_FRAME_ACK false
#define KMYC_ADAPTER_DSI_LANE_NUM 2
#define KMYC_ADAPTER_LANE_BITRATE_MBPS 750
#define KMYC_ADAPTER_LOG_TAG "waveshare_d070"
#define KMYC_ADAPTER_RESET_DESCRIPTION \
    "the P4-Pico DSI connector exposes no panel-reset GPIO"
