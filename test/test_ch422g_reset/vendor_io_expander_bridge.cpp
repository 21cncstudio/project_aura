// Compile the real masked-write API alongside the real CH422G port. The shared
// native esp_err mock does not define these unused error paths yet.
#ifndef ESP_ERR_INVALID_STATE
#define ESP_ERR_INVALID_STATE (-5)
#endif
#ifndef ESP_ERR_NOT_SUPPORTED
#define ESP_ERR_NOT_SUPPORTED (-6)
#endif

#include "../../third_party/ESP32_IO_Expander_Aura/src/port/esp_io_expander.c"
