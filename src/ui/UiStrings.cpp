// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiStrings.h"

#include <stddef.h>

namespace UiStrings {

namespace {

constexpr const char *kStringsEn[] = {
    "---",
    "--",
    "0%",
    "--:--",
    "--.--.----",
    "C",
    "F",
    "ppb",
    "Index",
    "HCHO",
    "AQI",
    "Initializing",
    "Fresh Air - All Good",
    "Excellent",
    "Good",
    "Moderate",
    "Poor",
    "OFF",
    "ON",
    "OK",
    "ERR",
    "SYNC",
    "Connected",
    "AP Mode",
    "Error",
    "Connecting",
    "Disabled",
    "No WiFi",
    "Connected",
    "Error",
    "Retrying (10m)",
    "Retrying (1h)",
    "Connecting...",
    "ON / OFF",
    "Every 6h",
    "http://192.168.4.1",
    "http://aura.local/mqtt",
    "http://aura.local/theme",
    "CO2 Very High! Ventilate now",
    "CO2 High - Ventilate soon",
    "CO2 Rising - Consider ventilation",
    "PM2.5 Very High! Purify - stop smoke",
    "PM2.5 High - Purify - check source",
    "PM2.5 Elevated - Consider purifier",
    "PM10 Very High! Purify - check source",
    "PM10 High - Purify - reduce dust",
    "PM10 Elevated - Purify - reduce dust",
    "HCHO Very High! Ventilate - remove source",
    "HCHO High - Ventilate - remove source",
    "HCHO Detected - Check sources",
    "VOC Very High! Find source - vent",
    "VOC High - Increase ventilation",
    "NOx Very High! Stop source - ventilate",
    "NOx High - Use hood - check source",
    "NOx Elevated - Check stove/outside",
    "Temp Too Cold! Heat now",
    "Temp Cold - Heat the room",
    "Temp Slightly Cool - Add heat",
    "Temp Too Hot! Cool now",
    "Temp Warm - Ventilate / cool",
    "Temp Slightly Warm - Increase airflow",
    "DP Very Low! Humidify",
    "DP Low - Air may feel dry",
    "DP Muggy! Dehumidify now",
    "DP Very High - Vent/dehumidify",
    "DP High - Air may feel muggy",
    "Humidity Extremely Low! Humidify",
    "Humidity Very Low - Use humidifier",
    "Humidity Low - Consider humidifier",
    "Humidity Extremely High! Dehumidify",
    "Humidity Very High - Dehumidify",
    "Humidity High - Increase airflow",
    "SETTINGS",
    "BACK",
    "TEMP OFFSET",
    "HUMID OFFSET",
    "NIGHT MODE",
    "C / F",
    "HEAD STATUS",
    "WI-FI",
    "TIME / DATE",
    "THEME COLOR",
    "MQTT",
    "AUTO NIGHT",
    "RESTART",
    "FACTORY\nRESET",
    "CO2\nCALLIBRATION",
    "ABOUT",
    "DOT\nINDICATORS",
    "ALERT\nBLINK",
    "VOC\nRELEARN",
    "BACKLIGHT"
};

constexpr const char *kStringsDe[] = {
    "---",
    "--",
    "0%",
    "--:--",
    "--.--.----",
    "C",
    "F",
    "ppb",
    "Index",
    "HCHO",
    "AQI",
    "Initialisiere...",
    "Frische Luft - Alles gut",
    "Ausgezeichnet",
    "Gut",
    "Mittel",
    "Schlecht",
    "AUS",
    "AN",
    "OK",
    "ERR",
    "SYNC",
    "Verbunden",
    "AP Modus",
    "Fehler",
    "Verbinde...",
    "Deaktiviert",
    "Kein WLAN",
    "Verbunden",
    "Fehler",
    "Wiederholen (10m)",
    "Wiederholen (1h)",
    "Verbinde...",
    "AN / AUS",
    "Alle 6h",
    "http://192.168.4.1",
    "http://aura.local/mqtt",
    "http://aura.local/theme",
    "CO2 Sehr hoch! Jetzt lüften",
    "CO2 Hoch - Bald lüften",
    "CO2 Steigt - Lüften erw.",
    "PM2.5 Sehr hoch! Reinigen - Rauch stoppen",
    "PM2.5 Hoch - Reinigen - Quelle prüfen",
    "PM2.5 Erhöht - Reiniger erw.",
    "PM10 Sehr hoch! Reinigen - Quelle prüfen",
    "PM10 Hoch - Reinigen - Staub reduzieren",
    "PM10 Erhöht - Staub reduzieren",
    "HCHO Sehr hoch! Lüften - Quelle weg",
    "HCHO Hoch - Lüften - Quelle weg",
    "HCHO Erkannt - Quellen prüfen",
    "VOC Sehr hoch! Quelle finden - lüften",
    "VOC Hoch - Mehr lüften",
    "NOx Sehr hoch! Quelle stoppen - lüften",
    "NOx Hoch - Abzug nutzen - Quelle prüfen",
    "NOx Erhöht - Herd/Außen prüfen",
    "Zu kalt! Heizen",
    "Kalt - Raum heizen",
    "Etwas kühl - Mehr Wärme",
    "Zu heiß! Kühlen",
    "Warm - Lüften / kühlen",
    "Etwas warm - Mehr Luft",
    "DP Sehr niedrig! Befeuchten",
    "DP Niedrig - Luft kann trocken sein",
    "DP Schwül! Entfeuchten",
    "DP Sehr hoch - Lüften/entf.",
    "DP Hoch - Luft kann schwül sein",
    "Luftfeuchte Extrem niedrig! Befeuchten",
    "Luftfeuchte Sehr niedrig - Befeuchter",
    "Luftfeuchte Niedrig - Befeuchter erw.",
    "Luftfeuchte Extrem hoch! Entfeuchten",
    "Luftfeuchte Sehr hoch - Entfeuchten",
    "Luftfeuchte Hoch - Mehr Luft",
    "EINSTELLUNGEN",
    "ZURÜCK",
    "TEMP-KORR.",
    "FEUCHT-KORR.",
    "NACHTMODUS",
    "C / F",
    "HEAD STATUS",
    "WI-FI",
    "ZEIT / DATUM",
    "THEME FARBE",
    "MQTT",
    "AUTO NACHT",
    "NEUSTART",
    "WERKS\nRESET",
    "CO2\nKALIBR.",
    "INFO",
    "PUNKT\nANZEIGE",
    "ALARM\nBLINK",
    "VOC\nNEU LERN.",
    "HINTERLICHT"
};

static_assert(sizeof(kStringsDe) / sizeof(kStringsDe[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: DE table size mismatch");

// TODO: fill translations. Null entries fall back to English.
constexpr const char *kStringsEs[static_cast<size_t>(TextId::Count)] = {};

static_assert(sizeof(kStringsEn) / sizeof(kStringsEn[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: EN table size mismatch");

Language g_language = Language::EN;

const char *const *tableFor(Language lang) {
    switch (lang) {
        case Language::DE: return kStringsDe;
        case Language::ES: return kStringsEs;
        case Language::EN:
        default:
            return kStringsEn;
    }
}

} // namespace

void setLanguage(Language lang) {
    g_language = lang;
}

Language language() {
    return g_language;
}

const char *text(TextId id) {
    const size_t index = static_cast<size_t>(id);
    const char *const *table = tableFor(g_language);
    const char *value = table ? table[index] : nullptr;
    if (!value || !*value) {
        value = kStringsEn[index];
    }
    return value ? value : "";
}

} // namespace UiStrings
