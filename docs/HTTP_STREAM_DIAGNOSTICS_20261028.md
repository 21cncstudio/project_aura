# HTTP-диагностика standalone Aura перед 1.2.1 — 2026-10-28

Статус: локальная реализация и программные проверки завершены. **Причина
[issue #158](https://github.com/21cncstudio/project_aura/issues/158) не установлена.**
Новых аппаратных результатов и допуска к выпуску этот отчёт не даёт.

Работа выполнена в `D:/21cncstudio/project_aura_aq/firmware-main`, на `main`,
от чистого `21d48a48b9f0275f2a23f31e3a6a3981ee3dd32b`. Все изменения
этого отчёта сохраняются одним локальным коммитом. Firmware Hub, архивы,
устройства, COM, remote refs, issue и выпуск не изменялись. Завершённый soak
не возобновлялся. При чтении issue уже был один ответ от 21cncstudio;
эта задача сообщений туда не отправляла.

## Основание и проверенные upstream-механизмы

- [Официальный HTTP Server API ESP-IDF 6.1](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_http_server.html):
  штатный `httpd_sess_set_send_override`, контракт send callback и chunked response.
  HTTP event API даёт общий код/событие, но не сохраняет точный errno failed send
  вместе с текущим ответом; обычного обработчика событий здесь недостаточно.
- [Исходник httpd_txrx.c](https://github.com/espressif/esp-idf/blob/fff9895c82d744c7237be8847347bdd1b07c6643/components/esp_http_server/src/httpd_txrx.c):
  локальный SDK `fff9895c82d744c7237be8847347bdd1b07c6643`, файл SHA256
  `db2e0165c4c6aaff43615f68357cd3adc4f2f3a0cea8fe988e72ff31f0735436`.
  `httpd_send_all` уже продвигает указатель при частичном успешном send;
  `httpd_resp_send_chunk` не сообщает вызывающему численное errno и может
  отказать после заголовка, части тела либо перед завершающим CRLF.
- Сравнены текущий main, теги `v1.1.5`, `v1.2.0` и соответствующее место
  `firmware-hub`: прежняя подмена любого неуспеха на `EIO` есть во всех.
  Готового решения в Hub нет; код из Hub не переносился.
- [Закрытый soak](STANDALONE_121_SOAK_RESULT_20261027.md) остаётся отдельным
  аппаратным свидетельством. Обрыв при AP-настройке и поздний timeout в нём
  не доказывают причину #158, нехватку RAM или регрессию именно 1.2.0.

Выбран небольшой наблюдатель через официальный hook. Он вызывает тот же
синхронный `send`, сохраняет его результат/errno немедленно и возвращает
категории SDK. Формирование HTTP, цикл частичных send, размеры chunks,
таймауты сервера, маршруты и сокетные настройки остаются существующими.
Отдельная библиотека или замена транспорта для такой диагностики не нужны.

## Поведение и границы данных

- В `WebHttpSendCapture` сохраняется оригинальный результат HTTP API отдельно
  от errno сетевого вызова. Старое errno после успешной операции или внутреннего
  отказа SDK не читается. В JSON недоступные `errno`/`sdk_error` равны `null`.
  Локальный отказ подготовки без вызова SDK не выдаётся за код SDK.
- Категории: `connection_reset`, `connection_closed`, `socket_timeout`,
  `socket_write_error`, `internal_error`, плюс существующие policy timeouts.
  Это наблюдение на отправляющей стороне, а не назначение виновника. Например,
  reset сам по себе не отличает закрытие клиента от сетевого сбоя.
  Для потоковых ответов прежняя строка `client_disconnected` заменена на
  нейтральную `connection_closed`; числовые значения старых enum не сдвинуты.
- Любой отказ SDK chunk помечается terminal, в том числе EAGAIN/EINTR/ENOBUFS.
  Writer не повторяет такой chunk целиком. После abort finalizer не отправляет
  завершающий chunk. Успешный ответ учитывается только после проверки последнего
  `httpd_resp_send_chunk(nullptr, 0)`, включая пустое тело. Отказы begin/setup
  также учитываются и не запускают отправку тела.
- Контекст захватывается до очистки request headers SDK: path максимум
  95 символов, адрес peer максимум 47, User-Agent максимум 79. Query/fragment
  удаляются, управляющие и не-ASCII символы заменяются на `?`, обрезка отмечена.
  Читается только User-Agent, не Authorization/Cookie/другие заголовки и не тело.
  Пароли, токены параметров запроса и MQTT credentials не записываются.
- `duration_ms` — длительность потоковой отправки с проверкой её завершения,
  без времени чтения/разбора входящего запроса. `sent` — сумма полностью
  принятых SDK chunks; `partial_body_bytes` — принятые send байты тела внутри
  отказавшего chunk; `wire_bytes` включает HTTP headers/framing. Эти счётчики
  не доказывают доставку, ACK или отображение страницы браузером. В частности,
  `sent == total` с `stage=finish` всё ещё является неуспешным HTTP-ответом.
- RAM снимается после возвращения SDK, до записи предупреждения: внутренние
  free и lifetime minimum через те же дешёвые ESP counters, что `/api/diag`.
  Heap walk, largest-block walk, PSRAM walk и опрос Wi-Fi из HTTP task не добавлены.
- Wi-Fi/MQTT берутся из существующего ConnectivityRuntime под его mutex.
  Добавлен scalar-only `statusSnapshot()`: режим, connected/enabled, состояние
  STA, RSSI/valid, scan, retry counters и ожидание времени MQTT TLS. Никаких
  String/секретов или вызовов менеджеров из HTTP task. В отчёте есть
  `snapshot_age_ms`: это последний опубликованный снимок, не атомарное
  измерение физической сети точно в такт send.

## История и обычный журнал

`web_stream.recent_failures` содержит четыре последние группы отказов,
сначала новую. Успехи их не удаляют. После restart/reset RAM-история пуста.
Существующие `last_*` сохранены как сведения о последнем запросе; для
разбора отказов надо использовать `recent_failures`, а не `last_errno`.

Объединяются только последовательные эквивалентные сбои. Сохраняются
`first_ms`, `last_ms`, точный count и свежий снимок последнего случая.
Все отказы увеличивают `abort_count`, независимо от ограничения журналирования.
Первая запись WARN немедленная; повторы этой группы — максимум раз в 60 секунд
при следующем событии, с суммарным `n`. Подавленные строки посчитаны в
`suppressed_failure_logs`. Без следующего события отдельного таймера/строки
summary нет; полный count группы уже доступен в отчёте.

Изменение errno/SDK/stage, клиента/UA/path, ожидаемых/переданных байтов,
состояния сети/MQTT, retry counters, наличия RAM/сети/RSSI, переход порогов
200 мс write / 10 с response / 5 с snapshot age / 32 KiB free RAM создаёт
новую видимую группу. Также не скрываются отклонения RAM на 8 KiB или RSSI
на 10 dBm от начала группы, включая постепенное накопление изменения.
Успех прерывает последовательность объединения: следующий отказ снова виден.

Hook остаётся на keep-alive сокете, а состояние сбрасывается перед новым
запросом. Для обычных ответов через `send()` и SDK error replies, которые
не входят в потоковую историю, сохранён исходный низкоуровневый WARN send.
Это изменение диагностирует streaming HTML/assets; оно не превращает все
HTTP parser/recv/JSON/OTA ошибки в записи `recent_failures`.

Отчёт скачивается прежним способом: `http://IP/diag` → **Download report**.
Он уже включает `/api/diag`, поэтому новая история попадает в скачанный JSON
без нового endpoint или отдельного способа выгрузки. В установленной 1.2.0
этой истории пока нет: старый отчёт полезен, но исходное errno из err=5
задним числом не восстановит. USB-лог может содержать соседнюю строку SDK.

## Проверки

1. Основной native environment: **1 005 / 1 005**, 110 выполненных suite,
   failure/error/skipped — 0. В том числе 38 HTTP-проверок в шести затронутых
   наборах, 13 новых сценариев. Проверены реальный writer/response/state/JSON,
   terminal EAGAIN после частичного chunk, отказ finish после всего тела/пустого
   тела, setup failure, отсутствие stale errno, reset состояния между запросами,
   раздельность успехов и истории, кольцо, wrap millis, группировка/изменения
   состояния и сохранность счётчиков. Тест владения строками JSON сначала
   поймал заимствование const array, затем прошёл после явного копирования
   строк через const char pointer (ArduinoJson 7.4.3).
2. Реальные локальные TCP-соединения на **127.0.0.1**, Windows/MinGW:
   `tools/test_http_stream_sdk.py` извлекает без изменения
   `httpd_send_all` и `httpd_resp_send_chunk` из выбранного SDK, компилирует их
   как C и использует production `WebHttpSendCapture`. Только request/session
   структуры и SDK event/log plumbing заменены host-shim. Winsock ошибки явно
   переводятся в host errno, числовые значения не переносятся на lwIP.

   | Сценарий | Результат последнего прогона |
   | --- | --- |
   | Полная передача 65 536 байт | Точное декодирование chunked body, без повторных байтов |
   | Медленный reader, 2 097 152 байта | Точное тело, измеренная задержка отправки 8 151 мс, без stale error |
   | RST до отправки | SDK `0xB006`, сохранён ECONNRESET, stage body |
   | Reader перестал читать | Через 260 мс ETIMEDOUT, сохранён SDK `0xB006`, sent 24 576, partial body 4 096 |
   | RST перед завершающим chunk | 512/512 body, но stage finish и abort; повторной отправки нет |

   Локальные timings и смещения не являются нормативами ESP32. Host slow-client
   timeout увеличен только в стенде; firmware остаётся с прежними 30 с send timeout.
   Сами SDK функции, механизм фиксации errno и запрет повторов проверены;
   работа native ESP send-hook/session wiring на lwIP пока подтверждена сборкой,
   а не аппаратным запуском.
3. **Обе IDF-сборки** `4_3` и `7_dual_i2c` PASS через
   `D:/21cncstudio/project_aura_aq/tools/build.ps1`, который вызывает
   `scripts/build_idf.ps1`. Image target/checksum/hash, RTC ABI и I2C linkage
   проверки прошли. Existing unused-function/Kconfig warnings сохранены в логах;
   новых ошибок сборки нет.
4. `tools/eez_ui_postprocess.py --check` PASS, `git diff -- src/ui` пуст,
   `git diff --check` PASS. EEZ Build вручную не запускался.

Driver/profile-specific native environments, не затронутые HTTP-изменением,
заново не запускались; 1 005 — основной environment, не прежние 1 154 из soak.

## Стоимость памяти и локальные артефакты

Измерено Xtensa compiler/DWARF, а не sizeof на ПК:

| Объект | До → после, байт |
| --- | --- |
| WebStreamFailure | 320, всего четыре = 1 280 |
| WebStreamState | 244 → 1 544 (+1 300, включая управление кольцом) |
| EspHttpRequest | 656 → 920 (+264, один объект на backend, не на сокет) |
| ConnectivityRuntime | 516 → 524 (+8) |
| WebTransferSnapshot | После 1 348 |

Разница размеров трёх постоянных объектов — **1 572 байта**. ELF BSS увеличился
на 1 312 байт (включая выравнивание), request heap-object — на 264: около
**1.6 KB** постоянной памяти. Дополнительных session allocations и dynamic
allocations для самой истории/снимка сбоя нет. Формирование JSON отчёта
использует существующий динамический ArduinoJson/String и временно потребляет
дополнительную память; её runtime peak на ESP32 здесь не измерялся.

Отдельная компиляция с `-fstack-usage`: handler lambda `/api/diag` — 1 488 байт,
`handleDiagData` — 2 704, `stream_response_body` — 688. Это размеры отдельных
frames, не полная глубина стека и не физический watermark. HTTP stack по-прежнему
12 288 байт; менять его размер не пришлось. Реальный запас при скачивании
полного отчёта остаётся пунктом аппаратной проверки.

Evidence root: `build/http-diagnostics-20261028/` в этом checkout (ignored).
Предыдущие BIN/ELF сохранены в `before-*`, проверенные — в `verified-*`.
`verified-artifacts.json` хранит SHA256 каждого installer asset и каждого
изменённого source/test/tool файла. BIN созданы из изменений поверх `21d48a48`,
перед их общим локальным коммитом; build IDs: `21d48a4-dirty` и
`21d48a4-7-dual-i2c-dirty`. Это кандидаты, не установленные и не опубликованные
артефакты; после коммита не следует выдавать пересобранный BIN за эти же байты.

| Профиль | BIN bytes | SHA256 |
| --- | ---: | --- |
| 4_3 | 4 651 776 | `183e0b4a05c730843ea89d2573610137ef05d2d0891be065e4c9b7ac22e39477` |
| 7_dual_i2c | 4 653 920 | `72377ada00c570c39d8eb8c1d6bbc9f712a93a548380af1d730b9f906370d9af` |

Логи: `native-all.log`, `verified-build-4_3.log`, `verified-build-7_dual_i2c.log`.
Native JSON: `.pio/native-tests/reports/20261028T151909Z-04290182/00-pio.json`.
TCP JSON/SDK hash: `sdk-probe-final/result.json`; размеры/frames:
`size-stack/result.json` и `size-stack/object-sizes.json`.

Повтор TCP-проверки (только ПК; по умолчанию отдельная timestamp-папка):

```powershell
& C:/Users/user/.platformio/penv/Scripts/python.exe tools/test_http_stream_sdk.py `
  --idf-path D:/21cncstudio/project_aura_aq/tools/idf61-sdk/esp-idf
```

## Что осталось для #158 и выпуска

После отдельного разрешения аппаратной работы:

- На точном кандидате проверить `/`, assets и `/diag` с браузером и curl:
  полная загрузка, медленное чтение, прерванная загрузка/закрытие, RST и
  stalled reader. Сверить новый JSON с USB log и сетевым захватом; проверить
  отсутствие повторного chunk, корректное закрытие/следующий запрос и keep-alive.
- Проверить IPv4/при используемой конфигурации IPv6 peer, длинный User-Agent,
  отсутствие query secrets в отчёте, четыре группы ошибок, затем успешный
  запрос и Download report: история должна сохраниться.
- Во время этих сценариев сопоставить snapshot age, Wi-Fi/RSSI и MQTT state,
  проверить reconnect и оперативную память/HTTP stack. Повторить AP setup
  на телефоне/ПК и профиль 7-inch: desktop loopback это не покрывает.
- Получить отчёт автора #158 и выяснить, загружается ли dashboard, есть ли
  фоновые polling/monitoring клиенты и продолжаются ли обрывы без браузерных
  вкладок. На 1.2.0 — отчёт плюс доступный USB log; новая диагностика требует
  отдельно разрешённого кандидата. Без этих данных нельзя назвать виновника
  или считать #158 исправленным.

Остальные release gates из итогового soak сохраняются: OTA/recovery,
cold cycles, SD, длительная стабильность и отдельная аппаратная квалификация
7-inch. Этот локальный коммит их не закрывает.
