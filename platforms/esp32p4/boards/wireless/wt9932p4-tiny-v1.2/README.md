# 启明云端 WT9932P4-TINY V1.2

厂家目录标识：wireless（用户确认）；原始板型写法：WT9932P4-TINY_1V2。
软件 ID：wireless-wt9932p4-tiny-v1.2。

继承旧工程记录：16 MB Flash、32 MB 200 MHz HEX PSRAM，
WT0132P4-A1 模组上报 ESP32-P4 silicon v1.3。
board.c 负责申请 MIPI D-PHY LDO channel 3 / 2500 mV。
板级 sdkconfig.defaults 保留 Flash、PSRAM 和芯片修订设置。

FUSB 连接、屏幕 reset 拉高和固定背光见对应 Adapter。
当前没有假定触摸引脚，也没有将 GPIO26 用于背光。
