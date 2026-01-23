// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#pragma once

namespace UiText {

inline constexpr const char kValueMissing[] = "---";
inline constexpr const char kValueMissingShort[] = "--";
inline constexpr const char kValueZeroPercent[] = "0%";
inline constexpr const char kTimeMissing[] = "--:--";
inline constexpr const char kDateMissing[] = "--.--.----";

inline constexpr const char kUnitC[] = "C";
inline constexpr const char kUnitF[] = "F";
inline constexpr const char kUnitPpb[] = "ppb";
inline constexpr const char kUnitIndex[] = "Index";

inline constexpr const char kLabelHcho[] = "HCHO";
inline constexpr const char kLabelAqi[] = "AQI";

inline constexpr const char kStatusInitializing[] = "Initializing";
inline constexpr const char kStatusAllGood[] = "Fresh Air - All Good";
inline constexpr const char kQualityExcellent[] = "Excellent";
inline constexpr const char kQualityGood[] = "Good";
inline constexpr const char kQualityModerate[] = "Moderate";
inline constexpr const char kQualityPoor[] = "Poor";

inline constexpr const char kStatusOff[] = "OFF";
inline constexpr const char kStatusOn[] = "ON";
inline constexpr const char kStatusOk[] = "OK";
inline constexpr const char kStatusErr[] = "ERR";
inline constexpr const char kStatusSync[] = "SYNC";

inline constexpr const char kWifiStatusConnected[] = "Connected";
inline constexpr const char kWifiStatusApMode[] = "AP Mode";
inline constexpr const char kWifiStatusError[] = "Error";
inline constexpr const char kWifiStatusConnecting[] = "Connecting";

inline constexpr const char kMqttStatusDisabled[] = "Disabled";
inline constexpr const char kMqttStatusNoWifi[] = "No WiFi";
inline constexpr const char kMqttStatusConnected[] = "Connected";
inline constexpr const char kMqttStatusError[] = "Error";
inline constexpr const char kMqttStatusRetry10m[] = "Retrying (10m)";
inline constexpr const char kMqttStatusRetry1h[] = "Retrying (1h)";
inline constexpr const char kMqttStatusConnecting[] = "Connecting...";

inline constexpr const char kMqttToggleLabel[] = "ON / OFF";
inline constexpr const char kNtpInterval[] = "Every 6h";

inline constexpr const char kWifiPortalUrl[] = "http://192.168.4.1";
inline constexpr const char kMqttPortalUrl[] = "http://aura.local/mqtt";
inline constexpr const char kThemePortalUrl[] = "http://aura.local/theme";

} // namespace UiText
