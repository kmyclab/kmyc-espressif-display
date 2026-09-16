idf_component_register(SRCS "main.c" "demo_service.c" "demo_ui.c"
                       INCLUDE_DIRS "."
                       REQUIRES kmyc_display kmyc_touch esp_timer lvgl esp_psram)
