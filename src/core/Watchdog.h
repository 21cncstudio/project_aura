#pragma once

#include <stdint.h>

namespace Watchdog {
    bool setup(uint32_t timeout_ms);
    void kick();
}
