# 测试方法

## 离线交互应用检查

```sh
python tools/kmyc.py check --preset wireless-p4-d070-bridge-v12-demo
python -m unittest discover -s tests
python tools/kmyc.py build --preset wireless-p4-d070-bridge-v12-demo
```

协议组件的独立 CRC/帧/TLV/mock-transport 测试入口见
[`kmyc_controller`](../components/kmyc_controller/README.md)。目录检查、mock 与编译
均不等于实机通过。旧 panel/touch presets 应同时回归构建。

GT911 唤醒状态机可用本地主机 C 编译器测试：

```sh
python tests/run_touch_lifecycle.py --cc clang
# 或：python tests/run_touch_lifecycle.py --cc zig cc
```

测试直接编译所选生产函数体，并模拟总线、GPIO 提交和等待，覆盖一次身份读取失败、
两次失败后恢复真实轮询，以及唤醒信号失败时保留休眠门禁；它不验证物理脉宽或芯片就绪时间。

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
