idf_build_get_property(kmyc_selection_file KMYC_SELECTION_FILE)
include("${kmyc_selection_file}")
idf_component_register(
    SRCS "../../platforms/esp32p4/ports/shared_i2c.c"
    INCLUDE_DIRS "../../platforms/common/include"
    PRIV_INCLUDE_DIRS ${KMYC_DISPLAY_INCLUDE_DIRS}
    REQUIRES esp_driver_i2c
)
