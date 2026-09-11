# Unmodified Espressif v5.5.5 LCD/legacy-I2C adapter, compiled against IDF 6.1.
# The exported name is scoped to Aura; all transactions still use IDF's driver.
idf_component_register(SRCS "esp_lcd_panel_io_i2c_v1.c"
    INCLUDE_DIRS "include" REQUIRES driver esp_lcd)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    esp_lcd_new_panel_io_i2c_v1=aura_lcd_new_panel_io_i2c_legacy)
target_compile_options(${COMPONENT_LIB} PRIVATE -include aura_lcd_i2c_legacy.h)
