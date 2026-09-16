#include "kmyc_i2c.h"
#include "board_config.h"

static i2c_master_bus_handle_t s_bus;

esp_err_t kmyc_board_acquire_i2c(i2c_master_bus_handle_t *bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;
    if (!s_bus) {
        const i2c_master_bus_config_t config = {
            .i2c_port = KMYC_BOARD_I2C_PORT,
            .sda_io_num = KMYC_BOARD_I2C_SDA_GPIO,
            .scl_io_num = KMYC_BOARD_I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        esp_err_t error = i2c_new_master_bus(&config, &s_bus);
        if (error != ESP_OK) return error;
    }
    *bus = s_bus;
    return ESP_OK;
}
