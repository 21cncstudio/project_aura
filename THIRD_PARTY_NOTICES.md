# Third Party Notices

This project depends on third-party software. Licenses are provided by their respective owners.
Review and update this file when dependencies change.

| Component | Source | License |
| --- | --- | --- |
| ESP32_Display_Panel | https://github.com/esp-arduino-libs/ESP32_Display_Panel | See upstream LICENSE |
| ESP32_IO_Expander | https://github.com/esp-arduino-libs/ESP32_IO_Expander | See upstream LICENSE |
| esp-lib-utils | https://github.com/esp-arduino-libs/esp-lib-utils | See upstream LICENSE |
| LVGL | https://github.com/lvgl/lvgl | See upstream LICENSE |
| PubSubClient | https://github.com/knolleary/pubsubclient | See upstream LICENSE |
| ArduinoJson | https://github.com/bblanchon/ArduinoJson | See upstream LICENSE |

Additional components
- The Arduino ESP32 core and ESP-IDF libraries are distributed under their respective licenses.
  See the PlatformIO packages referenced in platformio.ini.

Native ESP-IDF 6.1 migration dependencies are pinned in `main/idf_component.yml`
and resolved in `dependencies.lock`. LVGL 9.5.0 and ArduinoJson 7.4.3 retain
upstream MIT licenses. ESP32 Display Panel, IO Expander 1.1.1 and esp-lib-utils
0.3.0 retain upstream Apache-2.0 licenses. Vendored source licenses and
adaptation records are in `components/espressif__esp32_display_panel` and
`third_party/ESP32_IO_Expander_Aura`.

The EEZ source uses JetBrains Mono and Noto Sans SC source fonts. Their SIL
Open Font License texts are retained alongside the TTFs in `ui/fonts`.
