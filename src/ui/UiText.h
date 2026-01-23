// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

#include "ui/UiStrings.h"

namespace UiText {

inline const char *ValueMissing() { return UiStrings::text(UiStrings::TextId::ValueMissing); }
inline const char *ValueMissingShort() { return UiStrings::text(UiStrings::TextId::ValueMissingShort); }
inline const char *ValueZeroPercent() { return UiStrings::text(UiStrings::TextId::ValueZeroPercent); }
inline const char *TimeMissing() { return UiStrings::text(UiStrings::TextId::TimeMissing); }
inline const char *DateMissing() { return UiStrings::text(UiStrings::TextId::DateMissing); }

inline const char *UnitC() { return UiStrings::text(UiStrings::TextId::UnitC); }
inline const char *UnitF() { return UiStrings::text(UiStrings::TextId::UnitF); }
inline const char *UnitPpb() { return UiStrings::text(UiStrings::TextId::UnitPpb); }
inline const char *UnitIndex() { return UiStrings::text(UiStrings::TextId::UnitIndex); }

inline const char *LabelHcho() { return UiStrings::text(UiStrings::TextId::LabelHcho); }
inline const char *LabelAqi() { return UiStrings::text(UiStrings::TextId::LabelAqi); }

inline const char *StatusInitializing() { return UiStrings::text(UiStrings::TextId::StatusInitializing); }
inline const char *StatusAllGood() { return UiStrings::text(UiStrings::TextId::StatusAllGood); }
inline const char *QualityExcellent() { return UiStrings::text(UiStrings::TextId::QualityExcellent); }
inline const char *QualityGood() { return UiStrings::text(UiStrings::TextId::QualityGood); }
inline const char *QualityModerate() { return UiStrings::text(UiStrings::TextId::QualityModerate); }
inline const char *QualityPoor() { return UiStrings::text(UiStrings::TextId::QualityPoor); }

inline const char *StatusOff() { return UiStrings::text(UiStrings::TextId::StatusOff); }
inline const char *StatusOn() { return UiStrings::text(UiStrings::TextId::StatusOn); }
inline const char *StatusOk() { return UiStrings::text(UiStrings::TextId::StatusOk); }
inline const char *StatusErr() { return UiStrings::text(UiStrings::TextId::StatusErr); }
inline const char *StatusSync() { return UiStrings::text(UiStrings::TextId::StatusSync); }

inline const char *WifiStatusConnected() { return UiStrings::text(UiStrings::TextId::WifiStatusConnected); }
inline const char *WifiStatusApMode() { return UiStrings::text(UiStrings::TextId::WifiStatusApMode); }
inline const char *WifiStatusError() { return UiStrings::text(UiStrings::TextId::WifiStatusError); }
inline const char *WifiStatusConnecting() { return UiStrings::text(UiStrings::TextId::WifiStatusConnecting); }

inline const char *MqttStatusDisabled() { return UiStrings::text(UiStrings::TextId::MqttStatusDisabled); }
inline const char *MqttStatusNoWifi() { return UiStrings::text(UiStrings::TextId::MqttStatusNoWifi); }
inline const char *MqttStatusConnected() { return UiStrings::text(UiStrings::TextId::MqttStatusConnected); }
inline const char *MqttStatusError() { return UiStrings::text(UiStrings::TextId::MqttStatusError); }
inline const char *MqttStatusRetry10m() { return UiStrings::text(UiStrings::TextId::MqttStatusRetry10m); }
inline const char *MqttStatusRetry1h() { return UiStrings::text(UiStrings::TextId::MqttStatusRetry1h); }
inline const char *MqttStatusConnecting() { return UiStrings::text(UiStrings::TextId::MqttStatusConnecting); }

inline const char *MqttToggleLabel() { return UiStrings::text(UiStrings::TextId::MqttToggleLabel); }
inline const char *NtpInterval() { return UiStrings::text(UiStrings::TextId::NtpInterval); }

inline const char *WifiPortalUrl() { return UiStrings::text(UiStrings::TextId::WifiPortalUrl); }
inline const char *MqttPortalUrl() { return UiStrings::text(UiStrings::TextId::MqttPortalUrl); }
inline const char *ThemePortalUrl() { return UiStrings::text(UiStrings::TextId::ThemePortalUrl); }

inline const char *LabelSettingsTitle() { return UiStrings::text(UiStrings::TextId::LabelSettingsTitle); }
inline const char *LabelSettingsBack() { return UiStrings::text(UiStrings::TextId::LabelSettingsBack); }
inline const char *LabelTempOffsetTitle() { return UiStrings::text(UiStrings::TextId::LabelTempOffsetTitle); }
inline const char *LabelHumOffsetTitle() { return UiStrings::text(UiStrings::TextId::LabelHumOffsetTitle); }
inline const char *LabelNightMode() { return UiStrings::text(UiStrings::TextId::LabelNightMode); }
inline const char *LabelUnitsCF() { return UiStrings::text(UiStrings::TextId::LabelUnitsCF); }
inline const char *LabelHeadStatus() { return UiStrings::text(UiStrings::TextId::LabelHeadStatus); }
inline const char *LabelWifi() { return UiStrings::text(UiStrings::TextId::LabelWifi); }
inline const char *LabelTimeDate() { return UiStrings::text(UiStrings::TextId::LabelTimeDate); }
inline const char *LabelThemeColor() { return UiStrings::text(UiStrings::TextId::LabelThemeColor); }
inline const char *LabelMqtt() { return UiStrings::text(UiStrings::TextId::LabelMqtt); }
inline const char *LabelAutoNight() { return UiStrings::text(UiStrings::TextId::LabelAutoNight); }
inline const char *LabelRestart() { return UiStrings::text(UiStrings::TextId::LabelRestart); }
inline const char *LabelFactoryReset() { return UiStrings::text(UiStrings::TextId::LabelFactoryReset); }
inline const char *LabelCo2Calibration() { return UiStrings::text(UiStrings::TextId::LabelCo2Calibration); }
inline const char *LabelAbout() { return UiStrings::text(UiStrings::TextId::LabelAbout); }
inline const char *LabelLedIndicators() { return UiStrings::text(UiStrings::TextId::LabelLedIndicators); }
inline const char *LabelAlertBlink() { return UiStrings::text(UiStrings::TextId::LabelAlertBlink); }
inline const char *LabelVocRelearn() { return UiStrings::text(UiStrings::TextId::LabelVocRelearn); }
inline const char *LabelBacklight() { return UiStrings::text(UiStrings::TextId::LabelBacklight); }

} // namespace UiText
