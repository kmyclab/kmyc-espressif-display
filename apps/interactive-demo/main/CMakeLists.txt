idf_build_get_property(kmyc_selection_file KMYC_SELECTION_FILE)
include("${kmyc_selection_file}")
set(touch_dependencies "")
if(KMYC_TOUCH_ENABLED STREQUAL "1")
    list(APPEND touch_dependencies kmyc_touch)
endif()
idf_component_register(SRCS "main.c" "demo_service.c" "demo_ui.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS "../../../components/kmyc_touch/include"
                       REQUIRES kmyc_display ${touch_dependencies} esp_timer lvgl esp_psram)
target_compile_definitions(${COMPONENT_LIB} PRIVATE KMYC_DEMO_TOUCH=${KMYC_TOUCH_ENABLED})
