#pragma once

/* The existing link ties reset high and keeps the backlight on.
 * No GPIO reset, GPIO26 backlight control, or touch pins are assumed.
 */
#define KMYC_ADAPTER_COMMAND_ACK false
#define KMYC_ADAPTER_FRAME_ACK false
#define KMYC_ADAPTER_DSI_LANE_NUM 2
#define KMYC_ADAPTER_LANE_BITRATE_MBPS 800
#define KMYC_ADAPTER_LOG_TAG "wireless_dsi"
#define KMYC_ADAPTER_RESET_DESCRIPTION "RST is tied high on the adapter"
