# 测试方法

## 离线交互应用检查

```sh
python tools/kmyc.py check --preset wireless-p4-d070-bridge-v12-demo
python -m unittest discover -s tests
python tools/kmyc.py build --preset wireless-p4-d070-bridge-v12-demo
python tools/kmyc.py check --preset wireless-p4-d101-bridge-v12-demo
python tools/kmyc.py build --preset wireless-p4-d101-bridge-v12-demo
```

协议组件的独立 CRC/帧/TLV/mock-transport 测试入口见
[`kmyc_controller`](../components/kmyc_controller/README.md)。目录检查、mock 与编译
均不等于实机通过。旧 panel/touch presets 应同时回归构建。

GT911/GT9271 唤醒状态机和两种显示尺寸的几何辅助函数可用本地主机 C 编译器测试：

```sh
python tests/run_touch_lifecycle.py --cc clang
# 或：python tests/run_touch_lifecycle.py --cc zig cc
```

测试直接编译所选生产函数体，并模拟总线、GPIO 提交和等待，覆盖一次身份读取失败、
两次失败后恢复真实轮询、睡眠及唤醒信号失败路径、不同 Goodix 复位 profile，
以及 GT9271 从 sleep 命令到 wake 的至少59ms间隔。还验证两个尺寸的 buffer 大小、
溢出拒绝和边缘判断；它不验证物理脉宽或芯片就绪时间。

D101 实测应独立确认 800×1280 竖屏、ILI9881C 的 1000 Mbps/名义71 MHz 配置、GT9271
身份和坐标方向、五目标/边缘/多点/标记跟随、亮度、复位及 sleep/wake。保留旧 D070
回归；不能将另一开发板、另一 lane 速率或 D070 的实测结论转移到新组合。

Bridge D101 从 800 Mbps 调整至 1000 Mbps 的对照只改变 lane 速率，保持 UI、
porches、触控时序和控制器固件不变。先比较静态整屏是否仍错位，再检查局部刷新；
不能仅凭串口启动正常就判定错位已修复。

实机阶段必须先断电确认接线，分别记录 controller present/absent、亮度0/1/50/100、
四图样、五点与边缘、touch reset、sleep/wake和运行中离线；按首次错误停止相关路径，
不要重复刷写掩盖问题。未经测量不宣称时序、原子性、断电或24小时测试通过。

测试程序用于快速判断显示和触摸能否正常使用，不要求用户维护复杂的验收记录。

## 显示

运行 `panel-test` 后，确认画面能够显示，颜色、方向和边界基本正确即可。

- `*-panel` 显示面板内部测试画面。
- `*-colorbar` 显示 ESP32-P4 输出的彩条。

## 触摸

运行 `touch-test` 后：

1. 点击四角和中心，确认触点出现在手指附近。
2. 在屏幕上滑动，确认方向和跟随正常。
3. 需要多点功能时，同时放下多个手指确认即可。

发现偏移、镜像、断触或其他明显问题时，可以保留一张照片或短视频，并在
[GitHub Issues](https://github.com/kmyclab/kmyc-espressif-display/issues) 中提供
开发板、屏幕型号和使用的 preset。
