# 上电与烧录

本次迁移不执行烧录。开始物理调试前，操作者必须确认电源已关闭、
准确板卡/显示/触摸/转接板组合，以及串口和网络接入情况。
未使用网络也要明确说明；绝不带电插拔 MIPI 或触摸 FFC。

在工程工作区的 ESP-IDF 5.5.3 PowerShell 中先构建：

```powershell
python tools/kmyc.py build --preset wireless-p4-d101-panel-test
```

确认硬件、COM端口和固件组合后，从同一目录使用对应输出：

```powershell
idf.py -C apps/panel-test -B out/wireless-p4-d101-panel-test/build -p <PORT> flash monitor
```

将 `<PORT>` 替换为本机确认过的串口。Ctrl+] 退出串口监视。
默认应看到 BIST heartbeat，DPI 视频引擎关闭；不应把“正常循环日志”
解释成屏幕已显示正确画面。目视确认后才切换 menuconfig 的 BIST 选项。
当前 reset 拉高，MCU 重启不等于屏幕经历完整断电；冷启动验证单独记录。
