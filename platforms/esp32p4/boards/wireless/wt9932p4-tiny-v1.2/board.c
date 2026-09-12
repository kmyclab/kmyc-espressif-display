#include "esp_ldo_regulator.h"
#include "esp_log.h"

#include "kmyc_board.h"
#include "board_config.h"

static esp_ldo_channel_handle_t s_dsi_phy_ldo;

esp_err_t kmyc_board_power_display(void)
{
    if (s_dsi_phy_ldo != NULL) {
        return ESP_OK;
    }
    ESP_LOGI("wireless_tiny", "Powering MIPI D-PHY: LDO channel %d at %d mV",
             KMYC_DSI_PHY_LDO_CHANNEL, KMYC_DSI_PHY_LDO_VOLTAGE_MV);
    const esp_ldo_channel_config_t config = {
        .chan_id = KMYC_DSI_PHY_LDO_CHANNEL,
        .voltage_mv = KMYC_DSI_PHY_LDO_VOLTAGE_MV,
    };
    return esp_ldo_acquire_channel(&config, &s_dsi_phy_ldo);
}
