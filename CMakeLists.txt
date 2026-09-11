cmake_minimum_required(VERSION 3.22)

set(AURA_PROFILE "4_3" CACHE STRING "Aura hardware profile: 4_3 or 7_dual_i2c")
set_property(CACHE AURA_PROFILE PROPERTY STRINGS 4_3 7_dual_i2c)
if(NOT AURA_PROFILE MATCHES "^(4_3|7_dual_i2c)$")
    message(FATAL_ERROR "Unsupported AURA_PROFILE: ${AURA_PROFILE}")
endif()
set(IDF_TARGET esp32s3)
set(SDKCONFIG "${CMAKE_SOURCE_DIR}/sdkconfig.idf61.${AURA_PROFILE}")
set(SDKCONFIG_DEFAULTS "${CMAKE_SOURCE_DIR}/sdkconfig.defaults.idf61")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/idf_prepare.py"
        --profile "${AURA_PROFILE}" --build-dir "${CMAKE_BINARY_DIR}"
    RESULT_VARIABLE aura_prepare_result
    COMMAND_ERROR_IS_FATAL ANY)
include("${CMAKE_BINARY_DIR}/generated/aura-build.cmake")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/platformio.ini"
    "${CMAKE_SOURCE_DIR}/scripts/idf_prepare.py"
    "${CMAKE_SOURCE_DIR}/scripts/idf_build_env.py")
set(PROJECT_VER "${AURA_APP_VERSION}")
include_directories("${CMAKE_SOURCE_DIR}/idf/include"
    "${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/generated")
add_compile_definitions(${AURA_DEFINITIONS}
    ARDUINO_USB_MODE=1 ARDUINO_USB_CDC_ON_BOOT=1 CORE_DEBUG_LEVEL=1
    LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE LV_COLOR_16_SWAP=0)

project(aura_aq)
get_filename_component(aura_toolchain_dir "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(AURA_SIZE_TOOL NAMES xtensa-esp32s3-elf-size
    HINTS "${aura_toolchain_dir}" NO_DEFAULT_PATH REQUIRED)
# Display Panel's public headers include LEDC even for a custom backlight.
# IDF 6 no longer exposes all peripheral headers through the driver component.
target_link_libraries(__idf_espressif__esp32_display_panel PUBLIC idf::esp_driver_ledc)
# Arduino's Esp.cpp includes an IDF 6.1 HAL header with partially initialized
# register unions. Zero-initialization is intentional; keep the warning visible
# without promoting this one upstream diagnostic to an error.
target_compile_options(__idf_espressif__arduino-esp32 PRIVATE
    -Wno-error=missing-field-initializers)
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
    message(FATAL_ERROR "This migration build requires ESP-IDF 6.1")
endif()
add_custom_target(aura_prepare
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/idf_prepare.py"
        --profile "${AURA_PROFILE}" --build-dir "${CMAKE_BINARY_DIR}"
    VERBATIM)
add_dependencies(__idf_main aura_prepare)
add_custom_target(aura_checks ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/idf_verify.py"
        --profile "${AURA_PROFILE}" --build-dir "${CMAKE_BINARY_DIR}"
        --size-tool "${AURA_SIZE_TOOL}"
    DEPENDS gen_project_binary
    VERBATIM)
