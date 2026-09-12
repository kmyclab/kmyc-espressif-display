# 新增独立 C App

1. 在 apps/<短软件ID>/ 建立 CMakeLists.txt、main/main.c 和 main/CMakeLists.txt。
   每个 App 自己定义 app_main()，可拥有专用 components/。
2. 复用 cmake/kmyc_project.cmake 的 setup/verify 接入；保持标准 IDF 工程入口。
   按 panel-test 示例，在引入 IDF project.cmake 后、project() 前调用
   idf_build_set_property 注册 KMYC_SELECTION_FILE，使依赖预扫描也能读取配置。
3. 在 app.json 声明 target 范围、所需能力和 default_preset。
4. 把功能选项放 main/Kconfig.projbuild，默认值放 sdkconfig.defaults。
   SDK 管理依赖放 idf_component.yml，由组件管理器生成 dependencies.lock。
5. App 通过公共 C 接口访问硬件；不复制板卡、产品驱动或 GPIO 定义。
6. 为真实兼容组合增加 preset，运行工具校验、单元测试和独立构建。

LVGL、USB 副屏和 touch-test 尚未实现。需要这些功能时先补齐相应接口、
传输/内存契约和适配能力，再注册 App，不建立伪成功的空 App。
