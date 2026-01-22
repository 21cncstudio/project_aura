#include "core/InitConfig.h"

#include <math.h>

#include "config/AppConfig.h"

void InitConfig::normalizeOffsets(float &temp_offset, float &hum_offset) {
    temp_offset = lroundf(temp_offset * 10.0f) / 10.0f;
    hum_offset = lroundf(hum_offset);

    if (hum_offset < Config::HUM_OFFSET_MIN) {
        hum_offset = Config::HUM_OFFSET_MIN;
    } else if (hum_offset > Config::HUM_OFFSET_MAX) {
        hum_offset = Config::HUM_OFFSET_MAX;
    }
}
