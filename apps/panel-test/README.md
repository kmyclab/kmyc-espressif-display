# panel-test

独立 C ESP-IDF 工程；自己的 main/main.c 定义 app_main()。
应用仅调用 kmyc_display 诊断接口，不包含 GPIO、DSI HAL 或供应商初始化表。

默认 preset：wireless-p4-d101-panel-test；ESP-IDF 5.5.3。
默认 CONFIG_KMYC_PANEL_INTERNAL_BIST=y，保持迁移前屏内 BIST 行为。
关闭该选项才启动 DPI 视频并每两秒交替输出横向/纵向硬件色条。
名义刷新率只是时序计算值，BIST 模式不代表主控正在输出该帧率。

可以从本 App 目录使用标准 ESP-IDF 命令，无需把 App 放入统一 main：

```powershell
idf.py -B ../../out/wireless-p4-d101-panel-test/build build
idf.py -B ../../out/wireless-p4-d101-panel-test/build menuconfig
```

CMake 会从 app.json 读取默认 preset，使用外部产品/平台源码组件。
开发工作区可直接构建；尚不是可拷贝出工作区的客户独立交付包。
