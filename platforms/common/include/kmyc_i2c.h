#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

/* App startup is single-owner. Shared bus is retained for firmware lifetime.
 * Device clients borrow it, never delete it. IDF serializes individual transfers. */
esp_err_t kmyc_board_acquire_i2c(i2c_master_bus_handle_t *bus);
