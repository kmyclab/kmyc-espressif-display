# 新增显示产品

先遵循工作区 KMYC_DISPLAY_PRODUCT_NAMING.md 登记 D/T/DT 类型、
尺寸、原生分辨率、接口、结构和硬件修订，避免重复型号。

显示组件建立 display/<size>/<完整型号>/；包含 component.yaml、
README.md、VERSION、CHANGELOG.md 与必要驱动。供应商料号只作追溯映射。
在 driver/kmyc_panel/ 放组件 CMakeLists.txt 和所需 idf_component.yml，
源码和包含目录仍由 component.yaml 的 sources/include_dirs 列出。
触摸产品同理归档 touch/；通用控制器代码可复用，具体玻璃/坐标配置独立。
assembly/<size>/<完整型号>/ 只引用 D/T，不复制两者源码。

显示工作模式可以区分原生与实验模式，不随适配参数变动悄悄改产品身份。
Adapter 必须引用实际存在的模式；增加产品后先跑元数据校验，再进行
构建验证及断电确认后的物理调试。
