set(aura_vendor_dir "${CMAKE_CURRENT_LIST_DIR}/../../third_party/ESP32_IO_Expander_Aura")
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    set(aura_glob_options CONFIGURE_DEPENDS)
endif()
file(GLOB_RECURSE sources ${aura_glob_options}
    "${aura_vendor_dir}/src/*.c" "${aura_vendor_dir}/src/*.cpp")
idf_component_register(SRCS ${sources}
    INCLUDE_DIRS "${aura_vendor_dir}/src" "../../include"
    REQUIRES esp-lib-utils aura_i2c)
