# panel-test 显示测试

独立的 C 语言 ESP-IDF 应用，用于检查面板初始化和显示链路，不包含触摸功能。

| 模式 | 用途 | Waveshare v1.x preset |
| --- | --- | --- |
| 面板内部 BIST | 快速检查支持 BIST 的面板控制器 | `waveshare-pico-r1-d101-bist` |
| ESP32-P4 彩条 | 检查 10.1 寸完整 DSI 视频路径 | `waveshare-pico-r1-d101-panel` |
| ESP32-P4 彩条 | 检查 7 寸完整 DSI 视频路径 | `waveshare-pico-r1-d070-panel` |

从仓库根目录构建和烧录，例如：

```sh
python tools/kmyc.py build --preset waveshare-pico-r1-d070-panel
idf.py -C apps/panel-test -B out/waveshare-pico-r1-d070-panel/build -p <PORT> flash monitor
```

其他开发板和芯片版本见[硬件支持](../../docs/hardware-support.md)。串口运行正常不
代表显示通过，请按[测试方法](../../docs/testing.md)目视确认颜色、方向、边界和
稳定性。
