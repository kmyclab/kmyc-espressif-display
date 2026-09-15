# KMYC-T101-CTP-I2C-GT9271-G01-A1

| 项目 | 参数 |
| --- | --- |
| 产品类型 | 电容触摸屏 |
| 控制器 | GT9271 |
| 接口 | I2C |
| 最大触点 | 5 |
| 原始坐标范围 | 1280×800 |
| 玻璃/版本 | G01 / A1 |

ESP-IDF 驱动支持 `0x5D`、`0x14` 地址探测和五点触控报告。实际 I2C 引脚、
RESET/INT 和坐标方向由所选开发板 Adapter 决定。

Waveshare ESP32-P4-Pico 当前使用 GPIO8 SCL、GPIO7 SDA 和轮询模式，输出坐标
映射为 800×1280。这些引脚仅属于该 Adapter，不是触摸产品的固定引脚。

已适配开发板和各项实机状态见
[兼容性状态表](../../../docs/hardware-support.md#兼容性状态表)。版本变化见
[版本记录](CHANGELOG.md)。
