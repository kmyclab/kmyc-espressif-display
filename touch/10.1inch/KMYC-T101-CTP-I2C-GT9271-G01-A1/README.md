# KMYC-T101-CTP-I2C-GT9271-G01-A1

按现有产品登记保留 GT9271、G01 玻璃结构和 A1 硬件身份。
当前 ESP-IDF 工作区仅有元数据，没有触摸驱动适配或实物验证。
控制器通用代码后续放公共组件，具体总线、RESET/INT 与坐标变换在
产品配置/Adapter 中登记，不从 Raspberry Pi 的引脚配置推断 ESP32 接线。
