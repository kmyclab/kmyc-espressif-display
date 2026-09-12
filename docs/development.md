# 开发环境

当前 preset 固定 ESP-IDF 5.5.3。先进入相应 SDK PowerShell，再使用
tools/kmyc.py build/menuconfig；工具不会自动升级 SDK。
Python 标准库即可运行 list/check/select 和 tests，不需要激活 ESP-IDF。

## ESP-IDF 环境

按 Espressif 官方说明安装 ESP-IDF 5.5.3，并进入该版本的 PowerShell 或
shell 环境。确认 `IDF_PATH` 和工具链均来自同一 SDK 安装后，在仓库根目录运行：

```powershell
idf.py --version
python tools/kmyc.py build --preset wireless-p4-d101-panel-test
```

不要把本机 SDK、工具链、环境路径或 generated 文件加入提交或交付包。

每个 preset 的 out 目录包含其有效 sdkconfig；查看配置应以该文件为准。
改变 SDK 或硬件配方时使用新的 preset，或先归档旧输出，再重新配置。
不要删除整个工作区或全局 SDK 来清理某个构建。
