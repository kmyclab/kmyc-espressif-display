# Short software component name; full product identity stays in parent directories.
idf_build_get_property(kmyc_selection_file KMYC_SELECTION_FILE)
include("${kmyc_selection_file}")
idf_component_register(
    SRCS ${KMYC_PRODUCT_SOURCES}
    INCLUDE_DIRS ${KMYC_PRODUCT_INCLUDE_DIRS}
    REQUIRES esp_lcd
)
