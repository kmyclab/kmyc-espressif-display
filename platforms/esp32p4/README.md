# ESP32-P4

IDF target：esp32p4。当前唯一登记 SDK：ESP-IDF 5.5.3。
板卡按 boards/<manufacturer>/<model-and-revision>/ 管理。
ports/ 保存 P4 外设实现；adapters/ 保存板卡与具体产品的连接适配。

当前板卡：wireless/wt9932p4-tiny-v1.2；wireless = 启明云端。
默认配置支持旧工程记录的 P4 v1.3 芯片；不要套用 v3-only 配置。
新增其他芯片修订、SDK 版本或板卡需要独立构建与实物记录。
