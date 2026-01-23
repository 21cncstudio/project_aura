// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include <stdint.h>
#include "config/AppConfig.h"

namespace UiStrings {

using Language = Config::Language;

enum class TextId : uint16_t {
    ValueMissing,
    ValueMissingShort,
    ValueZeroPercent,
    TimeMissing,
    DateMissing,
    UnitC,
    UnitF,
    UnitPpb,
    UnitIndex,
    LabelHcho,
    LabelAqi,
    StatusInitializing,
    StatusAllGood,
    QualityExcellent,
    QualityGood,
    QualityModerate,
    QualityPoor,
    StatusOff,
    StatusOn,
    StatusOk,
    StatusErr,
    StatusSync,
    WifiStatusConnected,
    WifiStatusApMode,
    WifiStatusError,
    WifiStatusConnecting,
    MqttStatusDisabled,
    MqttStatusNoWifi,
    MqttStatusConnected,
    MqttStatusError,
    MqttStatusRetry10m,
    MqttStatusRetry1h,
    MqttStatusConnecting,
    MqttToggleLabel,
    NtpInterval,
    WifiPortalUrl,
    MqttPortalUrl,
    ThemePortalUrl,
    MsgCo2VeryHigh,
    MsgCo2High,
    MsgCo2Rising,
    MsgPm25VeryHigh,
    MsgPm25High,
    MsgPm25Elevated,
    MsgPm10VeryHigh,
    MsgPm10High,
    MsgPm10Elevated,
    MsgHchoVeryHigh,
    MsgHchoHigh,
    MsgHchoDetected,
    MsgVocVeryHigh,
    MsgVocHigh,
    MsgNoxVeryHigh,
    MsgNoxHigh,
    MsgNoxElevated,
    MsgTempTooCold,
    MsgTempCold,
    MsgTempSlightlyCool,
    MsgTempTooHot,
    MsgTempWarm,
    MsgTempSlightlyWarm,
    MsgDewPointVeryLow,
    MsgDewPointLow,
    MsgDewPointMuggy,
    MsgDewPointVeryHigh,
    MsgDewPointHigh,
    MsgHumidityExtremelyLow,
    MsgHumidityVeryLow,
    MsgHumidityLow,
    MsgHumidityExtremelyHigh,
    MsgHumidityVeryHigh,
    MsgHumidityHigh,
    LabelSettingsTitle,
    LabelSettingsBack,
    LabelTempOffsetTitle,
    LabelHumOffsetTitle,
    LabelNightMode,
    LabelUnitsCF,
    LabelHeadStatus,
    LabelWifi,
    LabelTimeDate,
    LabelThemeColor,
    LabelMqtt,
    LabelAutoNight,
    LabelRestart,
    LabelFactoryReset,
    LabelCo2Calibration,
    LabelAbout,
    LabelLedIndicators,
    LabelAlertBlink,
    LabelVocRelearn,
    LabelBacklight,
    Count
};

void setLanguage(Language lang);
Language language();
const char *text(TextId id);

} // namespace UiStrings
