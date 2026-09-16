idf_build_get_property(kmyc_selection_file KMYC_SELECTION_FILE)
include("${kmyc_selection_file}")
set(kmyc_bridge_dependencies "")
if(KMYC_BRIDGE_ENABLED STREQUAL "1")
    list(APPEND kmyc_bridge_dependencies kmyc_controller kmyc_touch kmyc_platform)
endif()
idf_component_register(
    SRCS ${KMYC_DISPLAY_SOURCES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${KMYC_DISPLAY_INCLUDE_DIRS}
    PRIV_REQUIRES esp_lcd esp_hw_support hal kmyc_panel ${kmyc_bridge_dependencies}
)
