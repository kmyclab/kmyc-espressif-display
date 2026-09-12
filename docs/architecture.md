# 架构与配置来源

本工作区由多个独立 C ESP-IDF App 组成，每个 App 自有 main/main.c。
无顶层 main，无按芯片复制 App 的完整工程。共享产品保持完整 KMYC 型号。
platforms/<idf_target>/ 隔离芯片实现，boards/ 隔离厂家、板型和修订。

## 当前依赖方向

panel-test → kmyc_display 公共诊断接口 → 选定 Adapter →
板级供电 / P4 外设实现 / KMYC 显示产品驱动。

应用不得直接包含产品私有头文件或访问 GPIO/芯片 HAL。
当前接口只覆盖单显示、单任务拥有的诊断能力，初始化失败后重启。
新增 LVGL/USB 时再定义帧缓冲所有权、完成通知、并发和销毁契约；
不要把诊断接口误认为已具备通用绘制或传输功能。

## 唯一来源

- app.json：App target 范围、所需能力、默认 preset。
- platform.json：支持的 SDK 版本及平台公共源文件。
- board.json：厂家、原始型号、target、板级源文件和必要配置约束。
- display/touch 的 component.yaml：产品型号及对应软件实现。
- assembly.yaml：显示、触摸、贴合方式引用，不复制驱动。
- adapter.json：具体板卡、显示模式、总成范围、能力与连接实现。
- presets/*.json：选择上述 ID；不重复维护引脚、命令表或产品数据。
- sdkconfig.defaults：依次加载公共、芯片、板级、App 默认配置。
- Kconfig：当前 App 的功能选项。
- out/<preset>/sdkconfig：此次构建的有效配置；默认文件不覆盖已有值。

.yaml 采用 JSON 子集，与现有 Raspberry Pi 产品目录一致。Python 标准库即可
解析。产品 VERSION/CHANGELOG 独立管理 ESP-IDF 软件版本；不重命名硬件型号。

描述文件保存源文件清单，由 tools/kmyc.py 生成 selection.cmake，
components/kmyc_display/CMakeLists.txt 编译选定源码。没有重复的板型分支表。
显示产品通过 driver/kmyc_panel/ 暴露短名称 IDF 组件，由该处的
idf_component.yml 声明供应商驱动依赖；App 不绑定 ILI9881C。
更换显示实现无需编辑 App 的供应商依赖。本轮只支持一次选定一个显示。
当前菜单选择入口是 tools/kmyc.py select；仅展示已有 preset 的兼容组合，
而非无条件允许任意笛卡尔积。IDF menuconfig 用于已选组合的功能参数。

## 构建隔离

每个 preset 固定 App、SDK、target、板卡、显示、Adapter。
输出在 out/<preset>/，包括独立 sdkconfig、build、generated 和 build-info。
更改配置配方且已有 sdkconfig 时拒绝继续，提示新建 preset 或归档旧输出，
避免默认配置看似更新、实际继续使用旧值。

每个 App 的 dependencies.lock 由组件管理器维护。当前只接入 P4/5.5.3。
新增 target/SDK 前必须建立相应锁文件选择和 managed_components 隔离机制，
不能让不同 target/SDK 并发重写同一个 App 的依赖状态。
同一 preset 不支持并发构建。out 隔离本身不等于全部依赖已隔离。

## 支持边界

现有 D 产品编号保留 DSI4L；当前 Adapter 采用原工程两 lane 实验模式。
元数据兼容、编译通过、串口正常、目视正常、完整冷启动通过分别记录。
GT9271/OCA 总成只有身份登记；微雪、S3、S31、LVGL、USB App 尚未实现。
新增实现时才进入可构建目录索引，README 规划目录不表示支持。
