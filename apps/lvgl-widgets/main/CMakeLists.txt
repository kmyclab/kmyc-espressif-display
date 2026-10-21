idf_component_register(SRCS "main.c" "lvgl_port.c"
                       INCLUDE_DIRS "."
                       REQUIRES kmyc_display kmyc_touch esp_timer esp_psram lvgl)
