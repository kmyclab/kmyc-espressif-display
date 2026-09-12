# 新增主板

1. 核实厂家、原始板型、硬件版本、芯片与 SDK；wireless 表示启明云端。
2. 在 platforms/<idf_target>/boards/<厂家>/<型号版本>/ 注册 board.json。
   板卡 ID 稳定，原始厂商型号单独保存。
3. 分离芯片通用能力与板卡供电、Flash/PSRAM、引脚配置。
4. 新建 Adapter，明确产品模式、接线、转接板、触摸和资源冲突。
   只有实际实现的能力才能列入 capabilities；未核实项写 null。
5. 建立 preset，运行 check、tests 和 build。更新调试状态，不宣称实物已通过。

新 target 还需登记 platform.json、SDK 版本、源文件、依赖隔离机制。
从兼容的公共接口复用 App，不复制整个 App 目录。
