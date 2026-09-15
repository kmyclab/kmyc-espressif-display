# 硬件支持

这是仓库中唯一记录兼容状态的文件。

- ✅ 可用：已经在实物上正常使用。
- 🧪 待确认：代码可以构建，尚未确认实物。
- — 未适配：当前没有对应功能。

## 兼容性状态表

| 屏幕 / 总成 | 开发板 / 芯片 | 显示 | 触摸 | Preset |
| --- | --- | --- | --- | --- |
| CC10128007-31C / KMYC-DT101 | Waveshare ESP32-P4-Pico / v1.x | ✅ | ✅ | `waveshare-pico-r1-d101-panel`、`waveshare-pico-r1-d101-bist`、`waveshare-pico-r1-d101-touch` |
| CC10128007-31C / KMYC-DT101 | Waveshare ESP32-P4-Pico / v3.x | 🧪 | 🧪 | `waveshare-pico-r3-d101-panel`、`waveshare-pico-r3-d101-bist`、`waveshare-pico-r3-d101-touch` |
| HYY7BIPS27A / KMYC-D070 + KMYC-T070 | Waveshare ESP32-P4-Pico / v1.x | ✅ | ✅ | `waveshare-pico-r1-d070-panel`、`waveshare-pico-r1-d070-touch` |
| CC10128007-31C / KMYC-D101 | 启明云端 WT9932P4-TINY V1.2 / v1.3 | 🧪 | — | `wireless-p4-d101-panel-test` |

表中的短型号对应：

- `KMYC-D101`：`KMYC-D101-DSI4L-800X1280-A1` 显示屏。
- `KMYC-DT101`：`KMYC-DT101-DSI4L-800X1280-GT9271-OCA-G01-A1` 显示触摸总成。
- `KMYC-D070`：`KMYC-D070-DSI4L-1024X600-A1` 显示屏。
- `KMYC-T070`：`KMYC-T070-CTP-I2C-GT911-G01-A1` 独立触摸产品；总成型号尚未登记。

## 产品

| 类型 | KMYC 型号 | 主要规格 |
| --- | --- | --- |
| 显示 | `KMYC-D101-DSI4L-800X1280-A1` | 10.1 英寸、800×1280、ILI9881C |
| 触摸 | `KMYC-T101-CTP-I2C-GT9271-G01-A1` | I2C、最多五点、GT9271 |
| 总成 | `KMYC-DT101-DSI4L-800X1280-GT9271-OCA-G01-A1` | 上述显示与触摸、OCA、G01/A1 |
| 显示 | `KMYC-D070-DSI4L-1024X600-A1` | 7 英寸、1024×600、JD9165BA |
| 触摸 | `KMYC-T070-CTP-I2C-GT911-G01-A1` | I2C、最多五点、GT911 |

产品型号登记为四通道 DSI。当前 ESP32-P4 Adapter 使用两通道连接，该连接由
对应 preset 选择。

## Waveshare ESP32-P4-Pico 接线

| 信号 | 连接 |
| --- | --- |
| 显示 | 板载 22-pin、两通道 MIPI-DSI |
| 触摸 SCL | GPIO8 |
| 触摸 SDA | GPIO7 |
| Flash / PSRAM | 32 MB / 32 MB |

10.1 寸模式为 RGB888、800×1280、71 MHz、每 lane 1000 Mbps；7 寸模式为
RGB888、1024×600、51 MHz、每 lane 750 Mbps。

官方资料：

- [Waveshare ESP32-P4-Pico 文档](https://docs.waveshare.net/ESP32-P4-Pico/)
- [Waveshare ESP32-P4 芯片修订配置](https://github.com/waveshareteam/ESP32-P4-Platform/blob/main/docs/ESP32P4_REVISION_CONFIG.md)
