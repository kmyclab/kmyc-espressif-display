# 2026-09-12 架构迁移验证

## 输入

- 基础：将原有单 App 工程迁移为独立 App、产品组件和平台 Adapter。
- App：panel-test，独立 C main/main.c。
- preset：wireless-p4-d101-panel-test。
- target / SDK：esp32p4 / ESP-IDF 5.5.3。
- 供应商组件：espressif/esp_lcd_ili9881c 1.1.0。
- 工具链：riscv32-esp-elf GCC 14.2.0，esp-14.2.0_20251107。
- 构建环境要求见 ../../development.md。

## 检查结果

1. tools/kmyc.py check：通过，完整 KMYC 产品目录与组件引用有效。
2. 11 项单元测试：通过。覆盖不兼容板卡、缺失能力、未知显示模式、
   未实现 App/总成、SDK 不匹配、路径越界、配置隔离和芯片修订约束。
3. select 交互选择：通过，只展示已登记的可构建组合，不自动烧录。
4. 完整固件构建及产品依赖拆分后的增量构建：通过。
5. 构建图确认 kmyc_display 依赖 kmyc_panel，供应商库由产品组件声明，
   App 的 main 不再声明 ILI9881C 依赖。
6. 迁移前后产品驱动 .c/.h 比对：仅规范化换行后内容完全一致；
   初始化命令、BIST 命令、两 lane 和时序参数没有改变。
7. 最终配置确认：BIST=y，周期 2000 ms，旧 P4 修订支持，16 MB Flash、
   200 MHz PSRAM。屏内 BIST 时 DPI 视频仍关闭。
8. dependencies.lock 由组件管理器因依赖归属迁移重新生成；SDK 和
   ILI9881C 版本未升级。out、下载组件和临时 SDK 被 Git 忽略。

## 产物

固件：out/wireless-p4-d101-panel-test/build/kmyc_panel_test.bin。
大小：246736 字节（0x3c3d0），应用分区 1 MiB。

SHA-256：

```text
10d6cb5ac0c2383f873a752577c9fc681d8c1c358d453a6e49f9e857491042df
```

有效 sdkconfig SHA-256：

```text
c13144f3a04f9ba0b6ba3af364332982ad1273dbc990c9c29ddf89f362ca2ef6
```

配方、锁文件校验和及源码状态另存 out/<preset>/build-info.json。
构建日志和其他生成产物位于 out/<preset>/，不进入 Git。

## 未进行的硬件操作

未烧录，未打开串口，未更换主板/显示/触摸/排线，未做目视或冷启动验证。
历史色条串口记录不能作为本次 BIST 实物验证。兼容状态继续为 experimental。
触摸与总成仅有身份登记，S3/S31 和其他 App 的接入仍在规划阶段。
