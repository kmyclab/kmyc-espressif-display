#pragma once
#include "kmyc_adapter.h"
esp_err_t kmyc_bridge_prepare(void);
bool kmyc_bridge_has_control(void);
esp_err_t kmyc_bridge_brightness(uint8_t duty);
esp_err_t kmyc_bridge_touch_sleep(void);
esp_err_t kmyc_bridge_touch_wake(void);
esp_err_t kmyc_bridge_fail_safe(void);
