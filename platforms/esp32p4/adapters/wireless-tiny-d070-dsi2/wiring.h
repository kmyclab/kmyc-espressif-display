#pragma once

/* The display connector exposes two DSI lanes with reset tied high and backlight on. */
#define KMYC_ADAPTER_COMMAND_ACK false
#define KMYC_ADAPTER_FRAME_ACK false
#define KMYC_ADAPTER_DSI_LANE_NUM 2
#define KMYC_ADAPTER_LANE_BITRATE_MBPS 750
#define KMYC_ADAPTER_LOG_TAG "wireless_d070"
#define KMYC_ADAPTER_RESET_DESCRIPTION "RST is tied high on the adapter"
