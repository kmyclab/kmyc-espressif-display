# touch-test 触摸测试

独立的 C 语言 ESP-IDF 应用，不使用 LVGL。它同时检查 RGB888 显示、触摸方向、
边缘范围和最多五点触控。

屏幕启动后显示网格、四色边框以及四角和中心五个目标：

- 点击位置显示彩色圆点，圆点应位于手指正下方。
- 命中的目标由橙色变为绿色。
- 多个手指同时按下时，每个触点显示不同颜色。
- 顶部五格显示当前点数，并保留达到过的最大点数。

从仓库根目录构建 Waveshare ESP32-P4-Pico v1.x 配置：

```sh
python tools/kmyc.py build --preset waveshare-pico-r1-d070-touch
idf.py -C apps/touch-test -B out/waveshare-pico-r1-d070-touch/build -p <PORT> flash monitor
```

其他开发板和芯片版本见[硬件支持](../../docs/hardware-support.md)。完整操作和通过
条件见[测试方法](../../docs/testing.md)。
