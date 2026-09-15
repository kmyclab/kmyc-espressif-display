# Shared build integration; each apps/<app> remains a separate IDF project.
set(KMYC_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
get_filename_component(KMYC_ROOT "${KMYC_ROOT}" ABSOLUTE)

macro(kmyc_project_setup app_id)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(NOT KMYC_PRESET)
        file(READ "${KMYC_ROOT}/apps/${app_id}/app.json" app_manifest)
        string(JSON KMYC_PRESET GET "${app_manifest}" default_preset)
    endif()
    set(KMYC_PRESET "${KMYC_PRESET}" CACHE STRING "KMYC build preset" FORCE)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${KMYC_ROOT}/tools/kmyc.py"
                configure --preset "${KMYC_PRESET}" --app "${app_id}"
        RESULT_VARIABLE selection_result
        OUTPUT_VARIABLE selection_output ERROR_VARIABLE selection_error
    )
    if(NOT selection_result EQUAL 0)
        message(FATAL_ERROR "KMYC selection failed: ${selection_output}${selection_error}")
    endif()
    set(KMYC_OUTPUT "${KMYC_ROOT}/out/${KMYC_PRESET}")
    set(KMYC_SELECTION_FILE "${KMYC_OUTPUT}/generated/selection.cmake"
        CACHE INTERNAL "Generated KMYC selection" FORCE)
    include("${KMYC_SELECTION_FILE}")
    if(DEFINED IDF_TARGET AND NOT IDF_TARGET STREQUAL KMYC_TARGET)
        message(FATAL_ERROR "Preset ${KMYC_PRESET} requires ${KMYC_TARGET}, not ${IDF_TARGET}")
    endif()
    if(DEFINED ENV{IDF_TARGET} AND NOT "$ENV{IDF_TARGET}" STREQUAL "${KMYC_TARGET}")
        message(FATAL_ERROR "Environment IDF_TARGET disagrees with preset")
    endif()
    set(IDF_TARGET "${KMYC_TARGET}" CACHE STRING "Target from board manifest" FORCE)
    set(SDKCONFIG "${KMYC_OUTPUT}/sdkconfig" CACHE FILEPATH "Isolated configuration" FORCE)
    set(SDKCONFIG_DEFAULTS "${KMYC_DEFAULT_FILES}")
    set(EXTRA_COMPONENT_DIRS "${KMYC_ROOT}/components/kmyc_display" "${KMYC_PRODUCT_DIR}")
    if(KMYC_TOUCH_ENABLED STREQUAL "1")
        list(APPEND EXTRA_COMPONENT_DIRS "${KMYC_ROOT}/components/kmyc_touch")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${KMYC_CATALOG_INPUTS} ${KMYC_DEFAULT_FILES} "${KMYC_ROOT}/tools/kmyc.py")
endmacro()

macro(kmyc_verify_configuration)
    if(NOT "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" STREQUAL "${KMYC_REQUIRED_IDF}")
        message(FATAL_ERROR "This preset requires ESP-IDF ${KMYC_REQUIRED_IDF}; SDK migration needs separate validation")
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${KMYC_ROOT}/tools/kmyc.py" verify-sdkconfig
                --preset "${KMYC_PRESET}" --file "${SDKCONFIG}"
        RESULT_VARIABLE config_result ERROR_VARIABLE config_error
    )
    if(NOT config_result EQUAL 0)
        message(FATAL_ERROR "KMYC board configuration mismatch: ${config_error}")
    endif()
endmacro()
