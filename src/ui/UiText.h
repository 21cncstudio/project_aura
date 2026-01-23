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
inline const char *LabelWifiSettingsTitle() { return UiStrings::text(UiStrings::TextId::LabelWifiSettingsTitle); }
inline const char *LabelWifiStatus() { return UiStrings::text(UiStrings::TextId::LabelWifiStatus); }
inline const char *LabelWifiHelp() { return UiStrings::text(UiStrings::TextId::LabelWifiHelp); }
inline const char *LabelWifiSsid() { return UiStrings::text(UiStrings::TextId::LabelWifiSsid); }
inline const char *LabelWifiIp() { return UiStrings::text(UiStrings::TextId::LabelWifiIp); }
inline const char *LabelWifiForget() { return UiStrings::text(UiStrings::TextId::LabelWifiForget); }
inline const char *LabelWifiReconnect() { return UiStrings::text(UiStrings::TextId::LabelWifiReconnect); }
inline const char *LabelWifiStartAp() { return UiStrings::text(UiStrings::TextId::LabelWifiStartAp); }
inline const char *LabelMqttSettingsTitle() { return UiStrings::text(UiStrings::TextId::LabelMqttSettingsTitle); }
inline const char *LabelMqttStatus() { return UiStrings::text(UiStrings::TextId::LabelMqttStatus); }
inline const char *LabelMqttHelp() { return UiStrings::text(UiStrings::TextId::LabelMqttHelp); }
inline const char *LabelMqttDeviceIp() { return UiStrings::text(UiStrings::TextId::LabelMqttDeviceIp); }
inline const char *LabelMqttBroker() { return UiStrings::text(UiStrings::TextId::LabelMqttBroker); }
inline const char *LabelMqttTopic() { return UiStrings::text(UiStrings::TextId::LabelMqttTopic); }
inline const char *LabelMqttReconnect() { return UiStrings::text(UiStrings::TextId::LabelMqttReconnect); }
inline const char *LabelDateTimeTitle() { return UiStrings::text(UiStrings::TextId::LabelDateTimeTitle); }
inline const char *LabelTimeZone() { return UiStrings::text(UiStrings::TextId::LabelTimeZone); }
inline const char *LabelNtpAutoSync() { return UiStrings::text(UiStrings::TextId::LabelNtpAutoSync); }
inline const char *LabelSetTime() { return UiStrings::text(UiStrings::TextId::LabelSetTime); }
inline const char *LabelSetTimeHours() { return UiStrings::text(UiStrings::TextId::LabelSetTimeHours); }
inline const char *LabelSetTimeMinutes() { return UiStrings::text(UiStrings::TextId::LabelSetTimeMinutes); }
inline const char *LabelSetDate() { return UiStrings::text(UiStrings::TextId::LabelSetDate); }
inline const char *LabelSetDateDay() { return UiStrings::text(UiStrings::TextId::LabelSetDateDay); }
inline const char *LabelSetDateMonth() { return UiStrings::text(UiStrings::TextId::LabelSetDateMonth); }
inline const char *LabelSetDateYear() { return UiStrings::text(UiStrings::TextId::LabelSetDateYear); }
inline const char *LabelTimeCard() { return UiStrings::text(UiStrings::TextId::LabelTimeCard); }
inline const char *LabelApplyNow() { return UiStrings::text(UiStrings::TextId::LabelApplyNow); }
inline const char *LabelRtc() { return UiStrings::text(UiStrings::TextId::LabelRtc); }
inline const char *LabelWifiChip() { return UiStrings::text(UiStrings::TextId::LabelWifiChip); }
inline const char *LabelNtpChip() { return UiStrings::text(UiStrings::TextId::LabelNtpChip); }
inline const char *LabelStatusTitle() { return UiStrings::text(UiStrings::TextId::LabelStatusTitle); }
inline const char *LabelTemperatureTitle() { return UiStrings::text(UiStrings::TextId::LabelTemperatureTitle); }
inline const char *LabelHumidityTitle() { return UiStrings::text(UiStrings::TextId::LabelHumidityTitle); }
inline const char *LabelPressureTitle() { return UiStrings::text(UiStrings::TextId::LabelPressureTitle); }
inline const char *LabelWarmup() { return UiStrings::text(UiStrings::TextId::LabelWarmup); }
inline const char *LabelConfirmVocButton() { return UiStrings::text(UiStrings::TextId::LabelConfirmVocButton); }
inline const char *LabelConfirmRestartButton() { return UiStrings::text(UiStrings::TextId::LabelConfirmRestartButton); }
inline const char *LabelConfirmResetButton() { return UiStrings::text(UiStrings::TextId::LabelConfirmResetButton); }
inline const char *LabelConfirmCancelButton() { return UiStrings::text(UiStrings::TextId::LabelConfirmCancelButton); }
inline const char *LabelConfirmTitleVoc() { return UiStrings::text(UiStrings::TextId::LabelConfirmTitleVoc); }
inline const char *LabelConfirmTextVoc() { return UiStrings::text(UiStrings::TextId::LabelConfirmTextVoc); }
inline const char *LabelConfirmTitleRestart() { return UiStrings::text(UiStrings::TextId::LabelConfirmTitleRestart); }
inline const char *LabelConfirmTextRestart() { return UiStrings::text(UiStrings::TextId::LabelConfirmTextRestart); }
inline const char *LabelConfirmTitleReset() { return UiStrings::text(UiStrings::TextId::LabelConfirmTitleReset); }
inline const char *LabelConfirmTextReset() { return UiStrings::text(UiStrings::TextId::LabelConfirmTextReset); }
inline const char *LabelThemeTitle() { return UiStrings::text(UiStrings::TextId::LabelThemeTitle); }
inline const char *LabelThemeCustom() { return UiStrings::text(UiStrings::TextId::LabelThemeCustom); }
inline const char *LabelThemePresets() { return UiStrings::text(UiStrings::TextId::LabelThemePresets); }
inline const char *LabelThemeExample() { return UiStrings::text(UiStrings::TextId::LabelThemeExample); }
inline const char *LabelThemeCustomInfo() { return UiStrings::text(UiStrings::TextId::LabelThemeCustomInfo); }
inline const char *LabelAutoNightTitle() { return UiStrings::text(UiStrings::TextId::LabelAutoNightTitle); }
inline const char *LabelAutoNightHint() { return UiStrings::text(UiStrings::TextId::LabelAutoNightHint); }
inline const char *LabelAutoNightStartTitle() { return UiStrings::text(UiStrings::TextId::LabelAutoNightStartTitle); }
inline const char *LabelAutoNightEndTitle() { return UiStrings::text(UiStrings::TextId::LabelAutoNightEndTitle); }
inline const char *LabelBacklightTitle() { return UiStrings::text(UiStrings::TextId::LabelBacklightTitle); }
inline const char *LabelBacklightHint() { return UiStrings::text(UiStrings::TextId::LabelBacklightHint); }
inline const char *LabelBacklightScheduleTitle() { return UiStrings::text(UiStrings::TextId::LabelBacklightScheduleTitle); }
inline const char *LabelBacklightPresetsTitle() { return UiStrings::text(UiStrings::TextId::LabelBacklightPresetsTitle); }
inline const char *LabelBacklightSleepTitle() { return UiStrings::text(UiStrings::TextId::LabelBacklightSleepTitle); }
inline const char *LabelBacklightWakeTitle() { return UiStrings::text(UiStrings::TextId::LabelBacklightWakeTitle); }
inline const char *LabelBacklightAlwaysOn() { return UiStrings::text(UiStrings::TextId::LabelBacklightAlwaysOn); }
inline const char *LabelBacklight30s() { return UiStrings::text(UiStrings::TextId::LabelBacklight30s); }
inline const char *LabelBacklight1m() { return UiStrings::text(UiStrings::TextId::LabelBacklight1m); }
inline const char *LabelBacklight5m() { return UiStrings::text(UiStrings::TextId::LabelBacklight5m); }
inline const char *LabelCo2CalibTitle() { return UiStrings::text(UiStrings::TextId::LabelCo2CalibTitle); }
inline const char *LabelCo2CalibStart() { return UiStrings::text(UiStrings::TextId::LabelCo2CalibStart); }
inline const char *LabelCo2CalibAscInfo() { return UiStrings::text(UiStrings::TextId::LabelCo2CalibAscInfo); }
inline const char *LabelCo2CalibFreshInfo() { return UiStrings::text(UiStrings::TextId::LabelCo2CalibFreshInfo); }
inline const char *LabelBootTapToContinue() { return UiStrings::text(UiStrings::TextId::LabelBootTapToContinue); }
inline const char *LabelBootDiagTitle() { return UiStrings::text(UiStrings::TextId::LabelBootDiagTitle); }
inline const char *LabelBootDiagSystemTitle() { return UiStrings::text(UiStrings::TextId::LabelBootDiagSystemTitle); }
inline const char *LabelBootDiagSensorsTitle() { return UiStrings::text(UiStrings::TextId::LabelBootDiagSensorsTitle); }
inline const char *LabelBootDiagAppLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagAppLabel); }
inline const char *LabelBootDiagMacLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagMacLabel); }
inline const char *LabelBootDiagResetLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagResetLabel); }
inline const char *LabelBootDiagHeapLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagHeapLabel); }
inline const char *LabelBootDiagStorageLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagStorageLabel); }
inline const char *LabelBootDiagI2cLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagI2cLabel); }
inline const char *LabelBootDiagTouchLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagTouchLabel); }
inline const char *LabelBootDiagSenLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagSenLabel); }
inline const char *LabelBootDiagSfaLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagSfaLabel); }
inline const char *LabelBootDiagRtcLabel() { return UiStrings::text(UiStrings::TextId::LabelBootDiagRtcLabel); }
inline const char *LabelBootDiagErrorsDetected() { return UiStrings::text(UiStrings::TextId::LabelBootDiagErrorsDetected); }
inline const char *BootDiagStorageOkConfig() { return UiStrings::text(UiStrings::TextId::BootDiagStorageOkConfig); }
inline const char *BootDiagStorageOkDefaults() { return UiStrings::text(UiStrings::TextId::BootDiagStorageOkDefaults); }
inline const char *BootDiagRecovered() { return UiStrings::text(UiStrings::TextId::BootDiagRecovered); }
inline const char *BootDiagDetected() { return UiStrings::text(UiStrings::TextId::BootDiagDetected); }
inline const char *BootDiagFail() { return UiStrings::text(UiStrings::TextId::BootDiagFail); }
inline const char *BootDiagStarting() { return UiStrings::text(UiStrings::TextId::BootDiagStarting); }
inline const char *BootDiagNotFound() { return UiStrings::text(UiStrings::TextId::BootDiagNotFound); }
inline const char *BootDiagLost() { return UiStrings::text(UiStrings::TextId::BootDiagLost); }

} // namespace UiText
