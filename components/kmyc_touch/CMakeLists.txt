idf_build_get_property(kmyc_selection_file KMYC_SELECTION_FILE)
include("${kmyc_selection_file}")
idf_component_register(
    SRCS ${KMYC_TOUCH_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${KMYC_TOUCH_INCLUDE_DIRS}
    PRIV_REQUIRES esp_driver_i2c
)
