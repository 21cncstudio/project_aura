# Aura AQ: исправления AP/HTTP, 2026-11-01

Последующая доработка и новые локальные проверки: [HTTP_AP_REPAIR2_20261101.md](HTTP_AP_REPAIR2_20261101.md). Этот документ сохранён как история предыдущего кандидата.

> **Статус изменён после независимой перепроверки: кандидаты отклонены для приёмки.** Лично воспроизведена потеря ответов 400/413 при непрочитанном теле запроса; подтверждены незакрытый scan recovery и замечания по FailClosed. До следующего этапа нужна доработка и новые сборки. [Результаты повторной сверки](HTTP_AP_FIX_RECHECK_20261101.md). Ниже — исторический отчёт первого кандидата.

Подтверждённые дефекты из [сравнения ревью](HTTP_AP_REVIEW_COMPARISON_20261101.md) исправлены локально на `main`, начиная с двойного закрытия. Добавлены регрессионные проверки с настоящим циклом сессий ESP-IDF; обе окончательные сборки успешны. Дополнительно найден и исправлен неверный владелец `global_user_ctx`, вызывавший освобождение самого backend при `httpd_stop()`.

Исходная база: `77bfb4b0209a57c4a511352743cd7eb55a385df1`; рабочий код до исправлений соответствовал опубликованной beta `1625a09`. Итог — незакоммиченные изменения в `D:/21cncstudio/project_aura_aq/firmware-main`, с сохранённым снимком изменённых файлов и хешами. Устройства, COM, прошивка, подпись, публикация и push не выполнялись. Hub-ветка и диагностический архив не менялись; автоматического merge после standalone cleanup нет.

**Причина конкретных жалоб Chris и #158 остаётся неустановленной.** Двойное закрытие появилось после 1.2.0, поэтому не объясняет их исходные случаи. Неверный deleter `global_user_ctx` есть и в исходниках тега `v1.2.0`, но штатного вызова stop в production нет: см. коррекцию в перепроверке. Исправление дефектов не подменяет воспроизведение пользовательского события.

## Изменения и проверка

| Область | Итоговое поведение | Проверка |
| --- | --- | --- |
| Закрытие HTTP-сессии | `stopClient()` идемпотентно помечает запрос. Синхронный handler возвращает `ESP_FAIL`, SDK закрывает текущую сессию один раз. Асинхронных `httpd_sess_trigger_close` в backend больше нет | Старый код сбрасывал следующего клиента при другой keep-alive сессии; исправленный проходит оба варианта, RST, setup/body/finish/plain-send failures и повторный keep-alive |
| Обычный send | Проверяются storage/status/type и результат отправки; после терминальной ошибки ответ не продолжается | Инъекция ошибки обычного ответа и проверка следующего клиента |
| Владение backend | SDK получает заимствованный `this` с пустым deleter вместо стандартного `free()` | До исправления 13 процессов прошли assertions, затем упали при teardown с `0xC0000005`; после исправления корректный teardown всех процессов и три stop/stop/begin/request цикла |
| UTF-8 SSID | В label и `data-ssid` исходные байты после HTML escaping; ASCII-only sanitizer исключён | Реальный Arduino String: кириллица/ASCII; native: кавычки/HTML escaping и сохранение вручную введённого UTF-8 имени |
| AP root с query | `/?x=1` и `/?` открывают Wi-Fi setup; dashboard сохраняет свой маршрут | Native и матрица настоящего HTTP-сервера |
| Rescan | Принятый запрос сразу формирует страницу с progress и polling, даже если взятый snapshot ещё старый; отказ очереди даёт 503 | Старый snapshot при принятом scan и последующая страница; возврат результата постановки команды проверен по коду |
| Подготовка Wi-Fi HTML | Предварительный расчёт размера, один проверяемый reserve, проверка полной длины. Подстановка читает только исходный шаблон. Ошибка даёт небольшой 503 и лог, а не пустой/усечённый 200 | Отказы реального WString allocator при root/save, нормальный контроль, token в SSID и некорректный шаблон |
| Сохранение настроек | Страница подтверждения готовится до записи настроек; при её OOM применение настроек не вызывается | Проверка порядка production handler; инъекция обоих отказов выделения save renderer. Полная запись NVS на плате не выполнялась |
| Отмена scan | Вызов `esp_wifi_scan_stop`, ожидание пользовательского SCAN_DONE после внутреннего callback Arduino, затем освобождение SDK/Arduino результатов. Новые scan и STA откладываются до завершения старого; неудачный stop повторяется не чаще раза в секунду | 17 проверок production WifiScanSession: поздний callback, повторный старт, ошибка stop, остановленный драйвер с событием в очереди, повтор отмены |
| Favicon и HEAD | Favicon отвечает пустым 204 без redirect; неизвестные маршруты имеют пустое тело 302/404. Метод, не поддерживаемый известным маршрутом, отвечает пустым 405, включая HEAD | Матрица AP/STA и отдельные assertions; data favicon в обеих Wi-Fi страницах |
| Бюджет отправки | 5 секунд на один chunk/finish или обычный ответ, общий для внутренних partial send вызовов SDK; прежние 30 секунд исключены | Зависший клиент: следующий получает 200 примерно через 4,9 с. Частичные записи по одному байту не перезапускают бюджет: завершение около 5,0 с. Рабочий медленный клиент получил точно 60 КиБ примерно за 6,4 с |
| Power save | Mutex охватывает счётчик владения и SDK get/set; HTTP/MQTT/OTA используют общий guard. Изменение штатного baseline на NONE не восстанавливает старый режим после освобождения lease | 9 проверок interleaving, вложенных владельцев, повторного release и ошибок SDK. Штатный режим остаётся WIFI_PS_NONE |
| Ожидание WakePowerGuard | HTTP ждёт допуск не более 3 с и закрывает соединение без payload, если допуска нет. Hardware guard не отменяется и не открывается HTTP-кодом | Idle, PreQuiet около 2 с, FailClosed, сохранение закрытого допуска и восстановление после действия владельца |

Полноценное captive discovery (DNS/DHCP/автоматическое открытие портала ОС) не добавлялось: в согласованном сравнении это отдельная возможность, а не доказанный дефект текущего HTTP. Политика аппаратного FailClosed не менялась; ограничено только ожидание HTTP-запроса.

## Результаты

- `native_test`: **1008/1008**, 110 suites; launcher проверил identity собранного Unity source. Отчёт `.pio/native-tests/reports/20261101T165538Z-bbecea59/00-pio.json`.
- После полного прогона добавлена ещё одна проверка ручного UTF-8 SSID: вся затронутая suite **13/13**, `.pio/native-tests/reports/20261101T171429Z-14d2a67a/00-pio.json`. Эти 13 не суммируются с 1008 как непересекающиеся тесты.
- Новый `tools/test_http_server_sdk.py`: **70/70 assertions в 15 группах**, все процессы exit 0, включая обычную остановку сервера. Источник — адаптированный предоставленный независимый стенд, не новое независимое ревью.
- Прежний `tools/test_http_stream_sdk.py`: **5/5 ожидаемых TCP-исходов** (включая намеренные reset/timeout).
- IDF **4_3 PASS**, **7_dual_i2c PASS** из окончательного firmware-кода; зависимости не менялись. Проверки I²C linkage, reset/stall и размера раздела прошли. Сборки выводят также существующие предупреждения LVGL/deprecated API и unused helpers; отсутствие всех warnings не заявляется.
- EEZ postprocess `--check` PASS; `git diff -- src/ui` пустой; `git diff --check` PASS.

Новый стенд компилирует настоящий `esp_http_server` (main/parse/sess/txrx/uri/control), `http_parser`, Arduino WString и production transport/portal/render/stream/guard/scan-код. ОС, RTOS и радио заменены host shim; транспорт Windows/Winsock, только loopback. NetworkManager, EEPROM/NVS, web UI bridge и реальная доставка Wi-Fi событий целиком на стенде не исполняются. Проверки scan используют production класс с управляемым порядком callback. Это не эмуляция ESP32 и не доказательство радиоповедения.

## Точные кандидаты

| Профиль | Build ID / target | Размер app BIN | SHA256 app BIN |
| --- | --- | ---: | --- |
| 4_3 | `77bfb4b-dirty` / `aura-aq-v1` | 3390144 | `29c955dcfb429f5427784aa2c59eaffd5d119672aa5bb7f7b8090883a0653bf3` |
| 7_dual_i2c | `77bfb4b-7-dual-i2c-dirty` / `aura-aq-7-v1` | 3392304 | `44654ef9108f8a135d35e766178a22d90532a9fd1b29f6647d4a0c1b82cd9daf` |

Сохранённые BIN: [4,3″](../build/http-ap-fixes-20261101/artifacts/4_3/aura_aq.bin), [7″](../build/http-ap-fixes-20261101/artifacts/7_dual_i2c/aura_aq.bin). Это локальные unsigned application images, не опубликованные пакеты установщика. `dirty` не идентифицирует дерево однозначно: для проверки использовать SHA256 и снимок исходников.

В `build/http-ap-fixes-20261101/artifacts/` также сохранены ELF, MAP, bootloader, partition table, littlefs, ota_data, SDK config, build identity и release-artifacts metadata обоих профилей. [Манифест](../build/http-ap-fixes-20261101/artifacts/manifest.json) проверен после копирования. Исходные кандидаты `before-build-4_3/` и `before-build-7_dual_i2c/` сохранены и повторно сверены с их прежними манифестами.

## Доказательства и повторный запуск

Все файлы ниже находятся в `build/http-ap-fixes-20261101/`:

- `red-close/` и `green-close/` — regression до/после; исходный независимый baseline дополнительно сохранён в `../http-ap-review-20261101/independent-review/`.
- `sdk-final/` — assertions до исправления borrowed context с ненулевым exit при teardown; `stop-crash-backtrace.txt` — SIGSEGV. Это не успешный финальный прогон.
- `sdk-verified/result.json` — окончательные исходы, команды компиляции и хеши использованных исходников; `s1.log`…`s15.log` — детали.
- `slow-reader-investigate/` — отвергнутый 64 КиБ fixture превышал capacity WString; окончательная проверка использует 60 КиБ и сравнивает все байты. Это была ошибка тестового fixture, не обрыв исправленной передачи.
- `chunk-sdk/result.json`, `native.log`, `manual-ssid.log`, `build-final-4_3.log`, `build-final-7_dual_i2c.log` — отдельные проверки.
- `final-source/manifest.json`, `final-source/files/`, `final-source/changes.diff` — точные изменённые файлы и Git-база, включая новые untracked-файлы.

Из active checkout, PowerShell:

```powershell
& scripts/run_tests.ps1 -Environment native_test
python -X utf8 tools/test_http_server_sdk.py --idf-path D:/21cncstudio/project_aura_aq/tools/idf61-sdk/esp-idf
python -X utf8 tools/test_http_stream_sdk.py --idf-path D:/21cncstudio/project_aura_aq/tools/idf61-sdk/esp-idf
& D:/21cncstudio/project_aura_aq/tools/build.ps1 -Line main -Profile 4_3 -Jobs 6
& D:/21cncstudio/project_aura_aq/tools/build.ps1 -Line main -Profile 7_dual_i2c -Jobs 6
python -X utf8 tools/eez_ui_postprocess.py --check
```

## Границы результата и следующий этап

Нужна отдельно разрешённая аппаратная проверка точных кандидатов: AP setup с телефона и ПК, query/rescan/save, UTF-8 сеть, переход AP→STA, отмена scan при Wi-Fi off/OTA, восстановление после обрыва клиента, медленная/слабая сеть, MQTT/HTTP/OTA одновременно, пробуждение экрана на обеих моделях. Нужны boot log и новые stream stage/errno записи, а не вывод из одного `err=5`.

5 секунд — проверенный host-бюджет отдельной операции отправки, а не гарантия времени всей страницы на радио. Длинная передача с прогрессом разрешена существующими profile stream budgets; синхронный сервер всё ещё может занимать задачу на время здоровой передачи. При невозможности доставки SCAN_DONE класс удерживает ownership, чтобы поздний callback не повредил новый scan; аппаратные сбои event loop и потеря событий этим host-тестом не покрываются. Проверяемая подготовка HTML относится к root/save renderers, не ко всем выделениям памяти приложения.

## Официальные основания

Сначала проверены официальный SDK и родственная Hub-реализация; в Hub были те же старые lifecycle/SSID/scan пути, готового совместимого исправления не нашлось. Backend сохранён на штатном esp_http_server: использованы его handler return contract, context deleter и send override вместо замены HTTP-библиотеки. [ESP-IDF 6.1 HTTP Server](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/protocols/esp_http_server.html).

Отмена и освобождение scan опираются на `esp_wifi_scan_stop` и `esp_wifi_clear_ap_list`; отдельный production класс нужен для согласования очереди callback Arduino с network task. [ESP-IDF 6.1 Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/network/esp_wifi.html). Официальное описание подтверждает SCAN_DONE после scan stop; возврат NOT_STARTED сам по себе не свидетельствует об отсутствии уже поставленного события. [Wi-Fi event description](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-guides/wifi-driver/overview.html#wifi-event-scan-done).
