# LVGL Widgets Demo

直接运行 **LVGL 9.2.2 官方 Widgets Demo**：保留 LVGL Logo、版本号、Profile / Analytics / Shop 页面、动画图表、滑块、日历和键盘。右下角显示 LVGL 的 FPS/负载统计，便于观察刷新；该数值不是屏幕物理刷新率。

这是独立的 C 应用，不依赖 `interactive-demo`，也没有复制或修改第三方演示源码。

## 编译与烧录

本预设对应启明云端 WT9932P4-TINY V1.2（ESP32-P4 v1.3）、Bridge V1.2，以及 CC10128007-31C 10.1 寸屏（ILI9881C + GT9271）。使用原生 800×1280 竖屏，不需要改 `main.c` 中的引脚。

在 ESP-IDF **5.5.3** 终端进入仓库根目录：

```powershell
python tools/kmyc.py check --preset wireless-p4-d101-bridge-v12-widgets
python tools/kmyc.py build --preset wireless-p4-d101-bridge-v12-widgets
idf.py -C apps/lvgl-widgets -B out/wireless-p4-d101-bridge-v12-widgets/build -D KMYC_PRESET=wireless-p4-d101-bridge-v12-widgets -p <PORT> flash monitor
```

将 `<PORT>` 替换为实际串口。首次构建需要下载依赖。换线前关闭开发板和屏幕电源，禁止热插拔 FFC。本预设只使用 Bridge V1.2，不适用于其他转接方式。

烧录后应直接进入官方演示：点击三个页签，拖动滑块，点击输入框调出键盘，观察图表动画。演示使用单指指针交互；多点触摸画点检查请使用 `touch-test` 或 `interactive-demo`。

如果出现触摸不可用提示，先检查触摸接线，再重新启动。画面本身会继续显示。支持状态仅维护在[硬件支持表](../../docs/hardware-support.md)。

## 修改自己的界面

- `main/main.c`：独立应用入口。
- `main/lvgl_port.c`：公共显示/触摸接口与 LVGL 的连接；用自己的界面创建函数替换 `lv_demo_widgets()` 即可。
- `sdkconfig.defaults`：启用 Widgets Demo、字体、RGB888 和性能显示；不需要修改 `managed_components`。

同一任务负责 LVGL 和触摸读取。两块 40 行 PSRAM 绘图缓冲在显示复制完成后才归还 LVGL；超时会停止界面，防止继续复用仍被使用的缓冲。界面内存使用 ESP-IDF 标准分配器，较大分配可使用 PSRAM。本例不承诺无撕裂，也不执行休眠、控制器固件升级或自动硬件复位。

官方演示及素材来自 [LVGL v9.2.2](https://github.com/lvgl/lvgl/tree/v9.2.2/demos/widgets)，遵循其 MIT 许可证；本工程适配代码遵循仓库许可证。
