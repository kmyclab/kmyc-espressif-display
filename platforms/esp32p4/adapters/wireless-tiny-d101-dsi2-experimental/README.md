# WT Tiny → KMYC D101，两 lane 实验适配

板卡：wireless-wt9932p4-tiny-v1.2。
显示：KMYC-D101-DSI4L-800X1280-A1。
物理转接板的正式编号尚未核实，adapter.json 中保留 null。

继承的连接条件：

- CLK、D0、D1 对应连接，不交换 P/N。
- 屏幕 reset 被拉高，使用 DCS 软件复位，随后等待 120 ms。
- 背光默认开启，不控制 GPIO26。
- 链路不返回命令应答，禁用 command ACK 和 DPI frame ACK。
- 使用已有两 lane 初始化表、800 Mbps/lane、71 MHz DPI 配置。
- 不初始化触摸；没有已验证的 DT 总成绑定。

这是实验适配，尚缺目视确认和完整冷启动验证。
变更排线、屏幕或主板前必须断电并确认准确组合及控制台连接。
状态见工作区 docs/validation/wireless-tiny-d101-dsi2-experimental/status.md。
