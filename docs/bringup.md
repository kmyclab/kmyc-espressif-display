# 开始使用

本页从环境检查开始，带你完成一次可复现的编译、烧录和实机测试。

## 1. 准备软硬件

- Python 3.9 或更高版本。
- ESP-IDF 5.5.3 及其工具链。
- [支持列表](hardware-support.md)中的开发板、显示屏和触摸屏组合。
- 数据线、供电和可用串口。

更换显示屏、触摸屏、转接板或 FFC 前必须关闭电源。禁止带电插拔 MIPI-DSI
或触摸 FFC。

打开 ESP-IDF 5.5.3 终端，在仓库根目录确认环境：

```sh
idf.py --version
python --version
```

`idf.py --version` 应显示 `v5.5.3`。其他版本目前不属于已登记配置。

## 2. 确认 ESP32-P4 芯片版本

Waveshare ESP32-P4-Pico 有 pre-v3 和 v3.x 两类芯片配置，两者不能混用。连接
开发板后运行：

```sh
esptool.py -p <PORT> chip_id
```

按输出选择 preset 前缀：

| 芯片输出 | Preset 前缀 |
| --- | --- |
| v1.x（当前实机为 v1.3） | `waveshare-pico-r1-` |
| v3.x | `waveshare-pico-r3-` |

如果不能确定版本，先不要烧录，参照开发板官方修订说明或在 Issue 中附上完整的
`chip_id` 输出。

## 3. 选择测试程序

```sh
python tools/kmyc.py list presets
```

每个结果都会显示 App、开发板、芯片 profile、显示、触摸和 SDK。兼容状态只在
[硬件支持](hardware-support.md)中维护。也可以
使用交互选择器：

```sh
python tools/kmyc.py select
```

选择器只给出建议的构建命令，不会自动编译或烧录。

`wireless-p4-d070-bridge-v12-demo` 是 Bridge V1.2 交互应用。当前 D070 + GT911 +
Bridge V1.2 已完成启动、亮度、触摸、一次 sleep/wake 和触点标记初测，仍处于
集成测试中；尚未完成冷启动/重启循环及 1 小时/24 小时耐久测试，初测不代表全面验证。
连接使用板级 GPIO7 SDA / GPIO8 SCL，共享 PY32 0x2C 和 GT911；勿套用双开发板
内部夹具接线。连接或更换硬件前仍必须断电确认 D070/GT911、adapter 版本、供电和
共地，再按[应用说明](../apps/interactive-demo/README.md)检查、构建和烧录。

| 目的 | 推荐 preset（ESP32-P4 v1.x） |
| --- | --- |
| 10.1 寸 MIPI-DSI 视频链路 | `waveshare-pico-r1-d101-panel` |
| 10.1 寸面板内部 BIST | `waveshare-pico-r1-d101-bist` |
| 10.1 寸显示与 GT9271 触摸 | `waveshare-pico-r1-d101-touch` |
| 7 寸 MIPI-DSI 视频链路 | `waveshare-pico-r1-d070-panel` |
| 7 寸显示与 GT911 触摸 | `waveshare-pico-r1-d070-touch` |

## 4. 检查并编译

以触摸测试为例：

```sh
python tools/kmyc.py check --preset waveshare-pico-r1-d070-touch
python tools/kmyc.py build --preset waveshare-pico-r1-d070-touch
```

每个 preset 使用独立的 `out/<preset>/` 目录，避免不同开发板和芯片版本共用
`sdkconfig`。编译结束后，固件和构建信息都在该目录内。

## 5. 烧录并查看串口

烧录前再次确认：开发板、芯片版本、显示/触摸总成、转接板和串口均与 preset
一致。然后运行：

```sh
idf.py -C apps/touch-test -B out/waveshare-pico-r1-d070-touch/build -p <PORT> flash monitor
```

把 `<PORT>` 替换为实际串口。按 `Ctrl+]` 退出串口监视。

如果使用其他 preset，`-C` 必须指向该 preset 显示的 App，`-B` 必须指向相同
preset 的构建目录。例如彩条测试使用：

```sh
idf.py -C apps/panel-test -B out/waveshare-pico-r1-d070-panel/build -p <PORT> flash monitor
```

## 6. 判断结果

编译通过、串口正常和实物通过是三件不同的事。显示与触摸的操作步骤和通过条件
见[测试方法](testing.md)；已知的实机结论见[硬件支持](hardware-support.md)。

遇到黑屏、触摸无响应、串口找不到或配置过期时，先看
[常见问题](troubleshooting.md)。
