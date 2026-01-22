#include "TimeMock.h"

static time_t g_epoch = 0;

void setNowEpoch(time_t epoch) {
    g_epoch = epoch;
}

void advanceEpoch(uint32_t delta_s) {
    g_epoch += static_cast<time_t>(delta_s);
}

time_t mockNow() {
    return g_epoch;
}
