#pragma once
#include <stdint.h>
// Only the OTA header type is needed; unused OTA entry points are discarded.
class WebUiBridge { public: enum class FirmwareUpdateScreenMode : uint8_t { Hidden=0,ConfirmPending,ConfirmAllowed,ConfirmDenied,Installing }; };
