#pragma once

/* Goodix GT9271 host recipe: INT low at RST rising selects address 0x5D.
 * Wake starts strictly more than 58ms after the sleep command; 59ms is the
 * minimum, measured from completion of the I2C attempt. Not MCU semantics. */
#define KMYC_BRIDGE_ADAPTER_ID "wireless-tiny-d101-bridge-v12-dsi2"
#define KMYC_BRIDGE_BOARD_NAME "WT9932P4-TINY V1.2"
#define KMYC_BRIDGE_DISPLAY_ID "KMYC-D101-DSI4L-800X1280-A1"
#define KMYC_BRIDGE_TOUCH_ID "KMYC-T101-CTP-I2C-GT9271-G01-A1"
#define KMYC_BRIDGE_TOUCH_CONTROLLER "GT9271"
#define KMYC_BRIDGE_LCD_RESET_LOW_MS 20
#define KMYC_BRIDGE_LCD_RESET_READY_MS 120
#define KMYC_BRIDGE_TOUCH_RESET_LOW_MS 20
#define KMYC_BRIDGE_TOUCH_RESET_HIGH_MS 6
#define KMYC_BRIDGE_TOUCH_RESET_READY_MS 194
#define KMYC_BRIDGE_TOUCH_SLEEP_MIN_MS 59
#define KMYC_BRIDGE_TOUCH_WAKE_HIGH_MS 3
