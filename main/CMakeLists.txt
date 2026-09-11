if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(aura_glob_options CONFIGURE_DEPENDS)
endif()
file(GLOB_RECURSE aura_sources ${aura_glob_options}
    "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")
list(FILTER aura_sources EXCLUDE REGEX "/src/ui/ui[.]c$")
list(FILTER aura_sources EXCLUDE REGEX "/src/web/WebTransportArduino[.]cpp$")
# The old PlatformIO baseline still needs the exact IDF 5.3.2 backport.
# IDF 6.1 uses and verifies its own upstream implementation.
list(FILTER aura_sources EXCLUDE REGEX "/src/core/EspRestartNoosBackport[.]c$")
# Board configuration callbacks refer back to application objects. Make those
# objects available regardless of archive order; section GC still removes
# unused functions, as in the original PlatformIO object-based app link.
idf_component_register(WHOLE_ARCHIVE SRCS ${aura_sources}
    INCLUDE_DIRS "../src" "../include" "${CMAKE_BINARY_DIR}/generated"
    REQUIRES arduino-esp32 esp32_display_panel lvgl arduinojson aura_lcd_i2c_legacy
        driver esp_driver_i2c esp_driver_gpio esp_driver_ledc esp_lcd esp_http_server esp_timer
        esp_wifi esp_event lwip app_update nvs_flash mqtt sdmmc fatfs network_provisioning ulp)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u aura_ota_image_identity")
# Keep the application on its established C++17 language contract while IDF
# builds its own components with the SDK's newer default.
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>)
