set(aura_panel_source "${CMAKE_SOURCE_DIR}/components/espressif__esp32_display_panel")
set(aura_panel_overlay "${CMAKE_BINARY_DIR}/generated/display-panel")
idf_component_get_property(aura_touch_core espressif__esp_lcd_touch COMPONENT_DIR)
idf_component_get_property(aura_touch_gt911 espressif__esp_lcd_touch_gt911 COMPONENT_DIR)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/idf_panel_overlay.py"
    --source "${aura_panel_source}" --destination "${aura_panel_overlay}"
    --touch-core "${aura_touch_core}" --touch-gt911 "${aura_touch_gt911}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/scripts/idf_panel_overlay.py"
    "${CMAKE_SOURCE_DIR}/scripts/idf_i2c_adapt.py"
    "${CMAKE_SOURCE_DIR}/scripts/idf_touch_adapt.py")
foreach(aura_property SOURCES INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(aura_values __idf_espressif__esp32_display_panel ${aura_property})
    if(aura_values)
        string(REPLACE "${aura_panel_source}/src" "${aura_panel_overlay}/src" aura_values "${aura_values}")
        if(aura_property STREQUAL "SOURCES")
            # One implementation and one ABI: the managed official components
            # replace both bundled C drivers; the generated headers forward to them.
            list(FILTER aura_values EXCLUDE REGEX "/drivers/touch/port/esp_lcd_touch(_gt911)?[.]c$")
        endif()
        set_property(TARGET __idf_espressif__esp32_display_panel PROPERTY ${aura_property} "${aura_values}")
    endif()
endforeach()
target_link_libraries(__idf_espressif__esp32_display_panel PUBLIC idf::aura_i2c
    idf::espressif__esp_lcd_touch idf::espressif__esp_lcd_touch_gt911)
