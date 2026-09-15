# 参与贡献

感谢你改进开发板、显示、触摸或测试应用。提交前请先阅读
[工程结构](docs/architecture.md)，并尽量让一个改动只解决一个明确问题。

## 报告问题

硬件问题需要足够信息才能复现。请至少提供：

- 开发板完整型号、PCB/模组版本和芯片 `chip_id` 输出。
- 显示、触摸、总成和转接板型号。
- 使用的 preset、ESP-IDF 版本和源码提交。
- 实际现象、预期现象以及必要的日志、照片或短视频。

发布日志和照片前请移除本机路径、Wi-Fi 凭据、客户信息和其他敏感内容。排线或
屏幕有变化时，请说明操作是否在断电状态下完成。

## 增加 App

1. 新建 `apps/<app>/`，提供 `CMakeLists.txt`、`main/main.c` 和 `app.json`。
2. 在 `app.json` 声明支持的 target、所需能力和默认 preset。
3. 通过 `kmyc_display`、`kmyc_touch` 等公共接口访问硬件。
4. 为实际组合增加 preset，并完成检查、编译和所需实机测试。

每个 App 都是独立 C ESP-IDF 工程。不要建立统一的大型 `main`，也不要在 App
中直接访问 GPIO、芯片 HAL 或供应商命令表。

## 增加开发板

1. 在 `platforms/<idf_target>/boards/<manufacturer>/<board>/` 建立板卡目录。
2. 在 `board.json` 登记厂家、完整型号、芯片、Flash、PSRAM 和板级实现。
3. 不可互换的芯片版本使用独立 profile 和 preset。
4. 在 Adapter 中登记该开发板与具体产品的接线、参数和能力。

启明云端的厂家目录名使用 `wireless`；WT9932P4-TINY V1.2 的物理版本必须保留。

## 增加显示、触摸或总成

1. 按完整 KMYC 型号建立 `display/`、`touch/` 或 `assembly/` 产品目录。
2. 在 `component.yaml` 或 `assembly.yaml` 中登记规格、实现和组成关系。
3. 供应商料号只用于追溯，不能替代 KMYC 产品型号。
4. 将 lane、GPIO、复位、背光和坐标变换放入 Adapter。
5. 调试阶段可在 preset 中直接选择独立触摸产品；贴合和修订确认后再建立总成。

供应商文档、初始化表或第三方源码只有在确认允许再分发后才能提交。

## 代码和提交

- 使用四空格缩进、`snake_case` C 标识符和大写宏。
- 不手工修改 `dependencies.lock`。
- 不提交 `out/`、`build/`、`managed_components/`、生成的 `sdkconfig`、日志、
  凭据、本机路径、客户资料或内部工程文件。
- 固件修改应分别记录“构建通过”和“实物通过”，不要用其中一个代替另一个。

提交前运行：

```sh
python tools/kmyc.py check
python -m unittest discover -s tests
python tools/kmyc.py build --preset <preset>
```

最后一个命令需要 ESP-IDF 5.5.3。如果兼容性发生变化，只更新
[硬件支持](docs/hardware-support.md)中的状态表，不在其他文档重复记录状态。
