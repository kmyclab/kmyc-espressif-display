# KMYC Espressif Display

面向乐鑫芯片的 KMYC 显示与触摸驱动、开发板适配和硬件测试程序。

通过一个 preset 选择开发板、屏幕和测试程序，不需要复制工程或修改
`main.c`。所有应用均为独立的 C 语言 ESP-IDF 工程；当前不依赖 C++ 或 LVGL。

支持的屏幕、开发板和当前状态统一维护在[硬件支持](docs/hardware-support.md)。

## 可以做什么

| 应用 | 用途 | 画面 |
| --- | --- | --- |
| `panel-test` | 检查面板初始化和 MIPI-DSI 显示链路 | 面板 BIST 或 ESP32-P4 彩条 |
| `touch-test` | 检查显示方向、单点、边缘和最多五点触控 | 网格、测试目标和实时触点 |

未来的 LVGL、USB 副屏等功能会作为独立应用加入，共用同一套开发板和屏幕驱动。

## 快速开始

需要 Python 3.9+ 和 ESP-IDF 5.5.3。克隆仓库后，在 ESP-IDF 终端进入仓库根目录：

```sh
git clone https://github.com/kmyclab/kmyc-espressif-display.git
cd kmyc-espressif-display
python tools/kmyc.py list presets
```

先按开发板和芯片版本选择 preset。下面以 ESP32-P4 v1.3 的 Waveshare
ESP32-P4-Pico 触摸测试为例：

```sh
python tools/kmyc.py check --preset waveshare-pico-r1-d101-touch
python tools/kmyc.py build --preset waveshare-pico-r1-d101-touch
idf.py -C apps/touch-test -B out/waveshare-pico-r1-d101-touch/build -p <PORT> flash monitor
```

把 `<PORT>` 替换为实际串口，例如 Windows 的 `COM11` 或 Linux 的
`/dev/ttyUSB0`。接线、芯片版本识别和完整步骤见[开始使用](docs/bringup.md)。

## 使用前须知

- 更换显示屏、触摸屏、转接板或 FFC 前必须关闭电源。
- 禁止带电插拔 MIPI-DSI 和触摸 FFC。
- Waveshare ESP32-P4-Pico 的 pre-v3 与 v3.x 配置不可混用。
- 构建结果位于 `out/<preset>/`，不应提交到 Git。

## 文档导航

| 我想做什么 | 去哪里 |
| --- | --- |
| 第一次编译和烧录 | [开始使用](docs/bringup.md) |
| 选择开发板、屏幕和 preset | [硬件支持](docs/hardware-support.md) |
| 判断显示和触摸是否通过 | [测试方法](docs/testing.md) |
| 处理黑屏、触摸无响应或构建错误 | [常见问题](docs/troubleshooting.md) |
| 理解目录和扩展方式 | [工程结构](docs/architecture.md) |
| 增加 App、开发板或屏幕 | [参与贡献](CONTRIBUTING.md) |

## 许可证

KMYC 编写的源码采用 [GPL-2.0-only](LICENSE) 许可证。第三方组件遵循各自的
许可证。
