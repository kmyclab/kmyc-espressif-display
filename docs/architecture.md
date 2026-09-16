# 工程结构

这套结构让 App、开发板和屏幕独立演进：增加测试程序时不复制驱动，增加开发板时
不复制 App，增加屏幕时不把型号判断堆进 `main.c`。

## 目录职责

```text
kmyc-display/
├── apps/          可独立编译的 C 应用，每个应用有自己的 main/main.c
├── components/    App 使用的公共显示和触摸接口
├── display/       显示产品身份、参数和驱动
├── touch/         触摸产品身份、参数和驱动
├── assembly/      显示与触摸总成的组成关系
├── platforms/     芯片、开发板、外设实现和物理连接适配
├── presets/       可以直接检查和编译的完整组合
├── tools/         选择、检查和构建工具
├── tests/         不依赖实物的工程检查
└── docs/          上手、支持、测试和设计文档
```

## 一次构建如何组成

```text
Preset
├── App：要运行的功能
├── Board + profile：开发板和不可混用的芯片配置
├── Display + 可选 Touch / Assembly：显示、独立触摸或已登记总成
├── Adapter：该开发板与产品的实际接线和参数
└── ESP-IDF：已登记的 SDK 版本
```

App 只调用公共显示/触摸接口。Adapter 提供 lane、GPIO、复位、背光和触摸坐标
变换；产品驱动负责 ILI9881C、GT9271 等器件本身。芯片 HAL 和 GPIO 不进入 App。

## 为什么 App 各自有 main

`apps/<app>/` 是完整的 ESP-IDF 工程，拥有自己的 `CMakeLists.txt` 和
`main/main.c`。目前有：

- `panel-test`：面板 BIST 和 ESP32-P4 硬件彩条。
- `touch-test`：RGB888 测试画面、单点和多点触摸。
- `interactive-demo`：LVGL 9.2.2、控制器状态、亮度、GT911 与休眠唤醒。

其他 USB 副屏和生产应用也应建立独立 App，共用已有公共组件。这样客户
拿到某个 App 时入口明确，不需要从一个大型 `main` 中关闭无关功能。

## 配置文件

| 文件 | 说明 |
| --- | --- |
| `app.json` | App 支持的芯片和所需能力 |
| `board.json` | 开发板、芯片 profile、存储器和板级源码 |
| `component.yaml` | 显示或触摸产品规格与实现 |
| `assembly.yaml` | 显示和触摸产品的组成关系 |
| `adapter.json` | 开发板与产品的连接、参数和能力 |
| `presets/*.json` | 可构建的 App + 硬件 + SDK 组合 |

工程调试时，preset 可以直接组合显示产品和独立触摸产品；只有贴合方式、玻璃结构
和硬件修订已经确认后，才登记为总成。不能为了让代码通过而虚构 `DT` 型号。

这些 `.yaml` 使用 JSON 语法子集，工具运行不依赖额外 YAML 包。只有已实现且能够
构建的组合才进入 preset；目录中不会用空文件表示“未来支持”。

## 当前接口边界

公共接口覆盖显示初始化、RGB888 区域绘制和轮询触摸。新增 `kmyc_platform`
由板级配置获取唯一 I²C bus，GT911/GT9271 和控制器均借用、不得自行删除。
`kmyc_controller` 是不包含器件时序的纯 C 协议客户端，仅读描述和控制原语；
V1.2 adapter 持有该实例及互斥，负责 LCD/GT911/PWM 的具体时序。

交互 App 的 service task 独占 I²C 与生命周期操作，LVGL task 只发 action queue
并读取互斥保护的模型。默认两块 40 行 PSRAM 缓冲由 LVGL 管理；DPI
`on_color_trans_done` 通知复制完成，LVGL 任务确认后才归还缓冲，ISR 不调用 LVGL。
百分比亮度 API 在 adapter 映射到 raw duty。缺少/不兼容控制器时不发控制帧，
沿用 DCS reset 和直接触控；运行中三次状态失败标记离线但不改变硬件输出。
此模型不是通用多主机/任意调用者并发支持，其他后台任务不得绕过 service owner。

具体扩展步骤见[参与贡献](../CONTRIBUTING.md)。
