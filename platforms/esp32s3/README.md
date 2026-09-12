# ESP32-S3（规划）

保留芯片分层位置，尚未登记 platform.json、板卡或构建 preset。
后续结构：boards/<厂家>/<板型版本>/、ports/、adapters/。
App 和 KMYC 产品目录共享，芯片专用外设适配在此实现。
S3 没有原生 MIPI-DSI 主机，现有 DSI 屏不能直接套用 P4 Adapter。
