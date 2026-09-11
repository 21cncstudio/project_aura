#include "core/AirQualityEngine.h"

#include <cmath>

#include "config/AppConfig.h"

namespace {

float map_float_clamped(float value, float in_min, float in_max, float out_min, float out_max) {
    if (in_max <= in_min) {
        return out_min;
    }
    float v = value;
    if (v < in_min) {
        v = in_min;
    }
    if (v > in_max) {
        v = in_max;
    }
    return out_min + (out_max - out_min) * (v - in_min) / (in_max - in_min);
}

int score_from_thresholds(float value, float min_val, float t_good, float t_mod, float t_poor) {
    if (value <= t_good) {
        return static_cast<int>(lroundf(map_float_clamped(value, min_val, t_good, 0.0f, 25.0f)));
    }
    if (value <= t_mod) {
        return static_cast<int>(lroundf(map_float_clamped(value, t_good, t_mod, 25.0f, 50.0f)));
    }
    if (value <= t_poor) {
        return static_cast<int>(lroundf(map_float_clamped(value, t_mod, t_poor, 50.0f, 75.0f)));
    }
    const float cap = t_poor * 1.5f;
    float score = map_float_clamped(value, t_poor, cap, 75.0f, 100.0f);
    if (score < 75.0f) {
        score = 75.0f;
    }
    if (score > 100.0f) {
        score = 100.0f;
    }
    return static_cast<int>(lroundf(score));
}

int score_from_co(float co_ppm) {
    if (co_ppm <= 0.0f) {
        return 0;
    }
    if (co_ppm < Config::AQ_CO_GREEN_MAX_PPM) {
        return static_cast<int>(lroundf(
            map_float_clamped(co_ppm, 0.0f, Config::AQ_CO_GREEN_MAX_PPM, 0.0f, 25.0f)));
    }
    if (co_ppm <= Config::AQ_CO_YELLOW_MAX_PPM) {
        return static_cast<int>(lroundf(map_float_clamped(co_ppm,
                                                          Config::AQ_CO_GREEN_MAX_PPM,
                                                          Config::AQ_CO_YELLOW_MAX_PPM,
                                                          80.0f,
                                                          90.0f)));
    }
    if (co_ppm <= Config::AQ_CO_ORANGE_MAX_PPM) {
        return static_cast<int>(lroundf(map_float_clamped(co_ppm,
                                                          Config::AQ_CO_YELLOW_MAX_PPM,
                                                          Config::AQ_CO_ORANGE_MAX_PPM,
                                                          90.0f,
                                                          100.0f)));
    }
    return 100;
}

void maybe_promote(AirQualityEngine::GroupEvaluation &group,
                   const AirQualityEngine::MetricEvaluation &metric) {
    if (!metric.valid) {
        return;
    }
    if (!group.valid || metric.score > group.score) {
        group.valid = true;
        group.score = metric.score;
        group.dominant_metric = metric.metric;
    }
}

} // namespace

namespace AirQualityEngine {

Band bandFromScore(int score) {
    if (score <= 25) {
        return Band::Excellent;
    }
    if (score <= 50) {
        return Band::Good;
    }
    if (score <= 75) {
        return Band::Moderate;
    }
    return Band::Poor;
}

MetricEvaluation evaluateMetric(Metric metric, const SensorData &data, bool gas_warmup) {
    MetricEvaluation result{};
    result.metric = metric;

    switch (metric) {
        case Metric::PM05:
            result.group = Group::Particulates;
            if (data.pm05_valid && std::isfinite(data.pm05) && data.pm05 >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.pm05,
                                                     0.0f,
                                                     Config::AQ_PM05_GREEN_MAX_PPCM3,
                                                     Config::AQ_PM05_YELLOW_MAX_PPCM3,
                                                     Config::AQ_PM05_ORANGE_MAX_PPCM3);
            }
            break;
        case Metric::PM1:
            result.group = Group::Particulates;
            if (data.pm1_valid && std::isfinite(data.pm1) && data.pm1 >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.pm1,
                                                     0.0f,
                                                     Config::AQ_PM1_GREEN_MAX_UGM3,
                                                     Config::AQ_PM1_YELLOW_MAX_UGM3,
                                                     Config::AQ_PM1_ORANGE_MAX_UGM3);
            }
            break;
        case Metric::PM25:
            result.group = Group::Particulates;
            if (data.pm25_valid && std::isfinite(data.pm25) && data.pm25 >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.pm25,
                                                     0.0f,
                                                     Config::AQ_PM25_GREEN_MAX_UGM3,
                                                     Config::AQ_PM25_YELLOW_MAX_UGM3,
                                                     Config::AQ_PM25_ORANGE_MAX_UGM3);
            }
            break;
        case Metric::PM4:
            result.group = Group::Particulates;
            if (data.pm4_valid && std::isfinite(data.pm4) && data.pm4 >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.pm4,
                                                     0.0f,
                                                     Config::AQ_PM4_GREEN_MAX_UGM3,
                                                     Config::AQ_PM4_YELLOW_MAX_UGM3,
                                                     Config::AQ_PM4_ORANGE_MAX_UGM3);
            }
            break;
        case Metric::PM10:
            result.group = Group::Particulates;
            if (data.pm10_valid && std::isfinite(data.pm10) && data.pm10 >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.pm10,
                                                     0.0f,
                                                     Config::AQ_PM10_GREEN_MAX_UGM3,
                                                     Config::AQ_PM10_YELLOW_MAX_UGM3,
                                                     Config::AQ_PM10_ORANGE_MAX_UGM3);
            }
            break;
        case Metric::CO2:
            result.group = Group::Ventilation;
            if (data.co2_valid && data.co2 > 0) {
                result.valid = true;
                result.score = score_from_thresholds(static_cast<float>(data.co2),
                                                     400.0f,
                                                     Config::AQ_CO2_GREEN_MAX_PPM,
                                                     Config::AQ_CO2_YELLOW_MAX_PPM,
                                                     Config::AQ_CO2_ORANGE_MAX_PPM);
            }
            break;
        case Metric::VOC:
            result.group = Group::ReactiveGas;
            if (!gas_warmup && data.voc_valid && data.voc_index >= 0) {
                result.valid = true;
                result.score = score_from_thresholds(static_cast<float>(data.voc_index),
                                                     0.0f,
                                                     static_cast<float>(Config::AQ_VOC_GREEN_MAX_INDEX),
                                                     static_cast<float>(Config::AQ_VOC_YELLOW_MAX_INDEX),
                                                     static_cast<float>(Config::AQ_VOC_ORANGE_MAX_INDEX));
            }
            break;
        case Metric::NOX:
            result.group = Group::ReactiveGas;
            if (!gas_warmup && data.nox_valid && data.nox_index >= 0) {
                result.valid = true;
                result.score = score_from_thresholds(static_cast<float>(data.nox_index),
                                                     1.0f,
                                                     static_cast<float>(Config::AQ_NOX_GREEN_MAX_INDEX),
                                                     static_cast<float>(Config::AQ_NOX_YELLOW_MAX_INDEX),
                                                     static_cast<float>(Config::AQ_NOX_ORANGE_MAX_INDEX));
            }
            break;
        case Metric::HCHO:
            result.group = Group::ToxicGas;
            if (data.hcho_valid && std::isfinite(data.hcho) && data.hcho >= 0.0f) {
                result.valid = true;
                result.score = score_from_thresholds(data.hcho,
                                                     0.0f,
                                                     Config::AQ_HCHO_GREEN_MAX_PPB,
                                                     Config::AQ_HCHO_YELLOW_MAX_PPB,
                                                     Config::AQ_HCHO_ORANGE_MAX_PPB);
            }
            break;
        case Metric::CO:
            result.group = Group::ToxicGas;
            if (data.co_sensor_present &&
                data.co_valid &&
                std::isfinite(data.co_ppm) &&
                data.co_ppm >= 0.0f) {
                result.valid = true;
                result.score = score_from_co(data.co_ppm);
            }
            break;
        case Metric::None:
        default:
            break;
    }

    return result;
}

Result evaluate(const SensorData &data, bool gas_warmup) {
    Result result{};
    result.pm.group = Group::Particulates;
    result.ventilation.group = Group::Ventilation;
    result.reactive_gas.group = Group::ReactiveGas;
    result.toxic_gas.group = Group::ToxicGas;

    const Metric metrics[] = {
        Metric::PM05,
        Metric::PM1,
        Metric::PM25,
        Metric::PM4,
        Metric::PM10,
        Metric::CO2,
        Metric::VOC,
        Metric::NOX,
        Metric::HCHO,
        Metric::CO,
    };

    for (Metric metric : metrics) {
        const MetricEvaluation evaluation = evaluateMetric(metric, data, gas_warmup);
        switch (evaluation.group) {
            case Group::Particulates:
                maybe_promote(result.pm, evaluation);
                break;
            case Group::Ventilation:
                maybe_promote(result.ventilation, evaluation);
                break;
            case Group::ReactiveGas:
                maybe_promote(result.reactive_gas, evaluation);
                break;
            case Group::ToxicGas:
                maybe_promote(result.toxic_gas, evaluation);
                break;
            case Group::None:
            default:
                break;
        }
    }

    // During SEN66 gas warmup, keep AQI unavailable unless a stable non-gas source
    // like particulates or CO2 is already ready. This prevents early AQI=0 from
    // appearing from HCHO/CO alone while the main gas stack is still warming up.
    if (gas_warmup && !result.pm.valid && !result.ventilation.valid) {
        return result;
    }

    GroupEvaluation *groups[] = {
        &result.pm,
        &result.ventilation,
        &result.reactive_gas,
        &result.toxic_gas,
    };
    GroupEvaluation *valid_groups[4] = {};

    for (GroupEvaluation *group : groups) {
        if (group->valid) {
            valid_groups[result.valid_group_count++] = group;
        }
    }

    if (result.valid_group_count == 0) {
        return result;
    }

    for (uint8_t i = 0; i + 1 < result.valid_group_count; ++i) {
        for (uint8_t j = i + 1; j < result.valid_group_count; ++j) {
            if (valid_groups[j]->score > valid_groups[i]->score) {
                GroupEvaluation *tmp = valid_groups[i];
                valid_groups[i] = valid_groups[j];
                valid_groups[j] = tmp;
            }
        }
    }

    float composite = static_cast<float>(valid_groups[0]->score);
    if (result.valid_group_count > 1) {
        composite += 0.20f * static_cast<float>(valid_groups[1]->score);
    }
    if (result.valid_group_count > 2) {
        composite += 0.10f * static_cast<float>(valid_groups[2]->score);
    }
    if (result.valid_group_count > 3) {
        composite += 0.05f * static_cast<float>(valid_groups[3]->score);
    }

    result.valid = true;
    result.score = static_cast<int>(lroundf(composite));
    if (result.score < 0) {
        result.score = 0;
    }
    if (result.score > 100) {
        result.score = 100;
    }
    result.band = bandFromScore(result.score);
    result.dominant_group = valid_groups[0]->group;
    result.dominant_metric = valid_groups[0]->dominant_metric;
    return result;
}

} // namespace AirQualityEngine
