# Аудит кандидата Aura AQ 7-inch, 2026-08-31

## Результат и границы

Локальный аудит объединённых изменений завершён: исходники, зависимости
дисплея, артефакты, OTA, тач/I2C, подсветка, диагностика и весь существующий
локальный набор тестов. В проверенных путях не выявлено нового P0/P1.
Найдены три существовавших ранее дефекта уровня P2 и один пробел сохранения
диагностики, который становится существеннее после правильного перевода
штатных GT911-сообщений в INFO. Они перечислены ниже и пока не исправлены.

Это не аппаратная квалификация выпуска. Причина редкого вертикального рывка
не установлена. Новейший кандидат не прошит. Исходники прошивки, частоты,
буферы, питание, проводка, настройки, опрос тача и установленные BIN в ходе
аудита не менялись. Выполнены только локальные проверки и GET-снимок 7-inch.
Не было reset, serial, COM8, OTA, публикации, push или изменения refs.

Аудит выполнен основным агентом и тремя независимыми ревьюерами по областям
display/RGB, touch/I2C/backlight и OTA/web/diagnostics. Native-тесты не исполняют
весь реальный путь UiController/FreeRTOS/RGB DMA. Слово «полный» здесь означает
весь текущий кандидат и имеющийся локальный набор проверок, а не доказательство
отсутствия всех возможных аппаратных, конкурентных или временных ошибок.

## Точный объект аудита

Worktree: `D:\21cncstudio\project_aura\tmp\worktrees\aura-post-115-clean`.
Ветка `main`, HEAD `5383b77e0dbad3338c02d67791a57acd8412b472`, dirty.
До аудита все 52 изменённых/новых авторских файла совпали с предыдущим
`AFTER.json`, включая 46 файлов вне docs. Это сохраняет идентичность исходников
уже собранного кандидата; новый документ аудита не меняет прошивку.

| Назначение | Environment | GT911 | BIN SHA256 |
| --- | --- | --- | --- |
| Новый 7-inch кандидат, не прошит | `project_aura_7_gt911_5d` | 0x5D | `900842aef79289e71ac76ae9b6d623023c89939cf46a4a86e8728cb964ba15c1` |
| Новый 4.3-inch регрессионный кандидат, не прошит | `project_aura` | 0x14 | `d514f276c93df11656c92d5e5c96b48eec47f28c6fcd18a2f4e1c4d8509dea92` |
| Обычный 7-inch environment, только сборка | `project_aura_7` | 0x14 | `80d3626a72537b466af4e6373c35a3b94999859a64c707996e1e072f99495122` |
| Последний документированный установленный 7-inch BIN | `project_aura_7_gt911_5d` | 0x5D | `1024a0c04fc17db411a8e6b865207827641651930427bdb43e85c80f64bfc547` |

Новый 7-inch BIN имеет размер 4 309 872 байта, profile `7_dual_i2c_scl6`,
target `aura-aq-7-v1`. У нового и установленного BIN одинаковая строка версии
`1.1.6-beta-5383b77-7-dual-i2c-scl6-gt911-5d-diag-dirty`.
По одной этой строке невозможно различить сборки. Нужны SHA и квитанция
конкретной установки. Назначение 0x5D будущим стандартом не превращает текущий
диагностический BIN в квалифицированный production build.

Артефакты и прежние три успешные свежие сборки:
`D:\21cncstudio\project_aura\logs\diagnostic_warning_policy_20260831T204001Z`.
Проверены хеши всех трёх BIN, manifest, ESP image checksum/hash, target descriptor,
сохранённые отчёты RTC ABI и restart-linker. В build-all.log не найдено строк
compiler warning/error. Повторная пересборка идентичного кандидата в аудит не
входила; полный native-прогон выполнен заново.

## Наблюдение пользователя и свежий снимок

Пользователь описал очень редкий одиночный небольшой вертикальный рывок, после
которого экран долго работает нормально. Уточнение: это происходит на главном
экране без касаний платы и без переключения страниц. Главный экран продолжает
обновлять показания и часы, поэтому отсутствие касаний не означает отсутствие
новых кадров. Точное время, включён ли `screen_flip_180`, и BIN в момент самого
события неизвестны. Связывать рывок с GT911, питанием или конкретной строкой
журнала пока нельзя.

GET-снимок 20:59:40 UTC, `192.168.1.165`, hostname `aura-f16e20`:

- Uptime 2035 s, build time `19:30:59`, SW boot, одна попытка Board begin,
  Board/LVGL ready, auto recovery false, подсветка ON без ожидающего перехода.
- Firmware/profile/target согласованы с последней записанной установкой
  `1024a0c0...`, но API не выдаёт SHA установленного BIN. Это не запуск `900842ae...`.
- I2C0 panel GPIO8/9; отдельный I2C1 sensors GPIO44/6. Сохранённый до init
  panel-снимок `idle`, SDA/SCL high, `live=false`.
- Основные подключённые датчики дают значения; CO=0, CO present, warmup=false,
  DAC available. Это один снимок, а не доказательство отсутствия потерь данных.
- В last_errors только те же четыре GT911 startup WARN и heartbeat 65441 ms:
  timeout=0, lock_fail=1, touch_err=0. Более поздних ошибок в данном API-снимке нет.
  В /api/events уже нет startup display-записей. Конечные кольца и выборочные
  снимки не доказывают нулевое количество низкоуровневых событий.

Снимок находится в `FOLLOWUP_7_20260831T205940652225Z` внутри evidence аудита.
`previous_backlight_trace` относится к предыдущей загрузке и не подтверждает
оптический результат текущей.

## Находки

### F1 / P2: единственный выходной буфер при повороте 180 градусов

В `src/lvgl_v8_port.cpp:1058` каждый повёрнутый кадр сначала копируется в один
`lvgl_port_rotated_fb`, затем вызывается switch/ожидание callback. Буфер назначен
из `fb[2]` при инициализации около строки 1295. После первого переключения он
остаётся источником непрерывного RGB-вывода. Следующий кадр начинает изменять
тот же источник до ожидания окончания передачи. При `screen_flip_180=true`
нарушено разделение буфера чтения дисплеем и записи CPU; возможен tearing.

Дефект уже есть в HEAD 5383b77, не внесён изменениями подсветки или уровней логов.
Применим и на main без касаний, когда обновляются данные. Настройка поворота
платы неизвестна и API её не выдаёт. Это не установленная причина рывка.
Исправление должно обеспечить отдельное владение выводимым и записываемым
повёрнутыми кадрами, с тестом последовательных обновлений и проверкой на плате.
Просто добавлять ожидание после существующей записи недостаточно.

### F2 / P2: ошибка чтения принимается за отпускание после wake

В `src/lvgl_v8_port.cpp:1461` условие `release_probe <= 0` снимает ожидание
чистого release и при отрицательном результате I2C. `lvgl_touch_note_error()`
около строки 628 также очищает этот флаг. После временного блока всё ещё
удерживаемый палец может стать обычным PRESSED без успешного RELEASE.

Это старый дефект HEAD 5383b77, не новая регрессия. Он может дать лишний click
или drag после wake с ошибкой транспорта. Описанный рывок нетронутого main
этим сценарием не объясняется. Нужны сохранение release-gate при ошибке и тест
`wake -> held -> read error -> held -> release -> new press`.

### F3 / P2: конкурентный доступ к диагностическим кольцам Logger

`src/core/Logger.cpp:51` изменяет запись/head/count, строка 77 копирует запись;
общие поля и alert sequence обновляются без синхронизации. NetworkPlane имеет
отдельную задачу, main/UI тоже пишет логи, HTTP читает снимки. Нет единого
владельца или lock для этих операций. Возможны потеря событий и несогласованные
поля снимка. Конкретное повреждение записей на плате не воспроизводилось.

Logger.cpp не менялся относительно HEAD. Связь с рывком не установлена.
Исправление: короткая синхронизация операций над кольцами и sequence, без
Serial/MQTT и форматирования внутри lock; отдельная конкурентная проверка.

### F4 / P2 диагностики: штатные startup INFO быстро теряются из отчёта

Новая политика правильно не считает рабочий 0x5D предупреждением. Но успешные
GT911 INFO перестают попадать в alert-буфер. `/api/events` в
`src/web/WebSystemApiHandlers.cpp:250` сначала берёт последние 48 сырых записей
Logger, а уже затем фильтрует события. INFO allowlist разрешает показ, но не
гарантирует сохранение всей истории запуска.

Есть пример на предыдущем BIN 1024a0c0: при uptime 10 s в
`INSTALL_7_ONCE/AFTER_EVENTS_POLLS.jsonl` осталась только последняя из четырёх
GT911-записей. Три первых тогда сохранялись только в
`FIRST_NEW_BOOT_DIAG.json.last_errors`. После перевода INFO этот резервный путь
исчезнет. Это доказанный путь потери данных с примером на прежней сборке,
а не аппаратное воспроизведение на новом 900842ae.

Нужно сохранять ограниченный структурированный startup snapshot отдельно от
обычного кольца. Возвращать штатные сообщения в WARN ради хранения неправильно.
Реальные ошибки выбранного адреса, timeout и неожиданный второй ответчик новая
политика по-прежнему оставляет WARN/ERROR.

## RGB, редкий рывок и пределы измерений

### Фактические параметры текущего кандидата

| Параметр | Значение |
| --- | --- |
| RGB | 800 x 480, RGB565, PCLK 16 MHz, falling edge |
| H/V pulse, back, front | 4 / 8 / 8 для обеих осей |
| Номинальный период scanout | 25.625 ms, около 39.02 Hz |
| Framebuffers | 3 x 768000 bytes в PSRAM |
| Bounce buffers | 2 x 16000 bytes, 10 строк каждый |
| DMA | burst 64, bb_invalidate_cache=0 |
| LVGL | direct mode 3, compile rotation 0, refresh 40 ms, core 1 / priority 2 |
| PSRAM | Octal 80 MHz, XIP/instruction fetch/rodata включены |
| Cache | I-cache 16 KiB / line 32 B; D-cache 32 KiB / line 64 B |

Параметры проверены по board config, BoardInit, pinned BusRGB и реальному SDK.
Номинальная частота вычислена из PCLK и суммарных porch; это не измерение
осциллографом. Последние изменения logging/backlight этих параметров не меняли.

### Восстановление драйвера может пройти без ошибки приложения

Исследован именно `libesp_lcd.a` из link-map кандидата, ESP-IDF 5.3.2, SHA256
`94babdbccb36109be9de3a9fdd758e8422ec49c91a363eb0f87e475105bb26b4`.
Disassembly/DWARF показывают в `rgb_lcd_default_isr_handler` ветку сравнения
`bb_eof_count < expect_eof_count`, восстановление позиции bounce и вызовы
`gdma_reset/gdma_start`. Отдельного события в наш Logger эта ветка не передаёт.

**Гипотеза, не диагноз:** краткое нарушение своевременной подачи RGB-данных с
автовосстановлением совместимо с одиночным рывком и продолжением работы.
Espressif описывает возможность сдвига/мерцания при опоздании bounce refill
из-за конкуренции за PSRAM. Это объясняет, почему такой механизм следует
проверять, но не доказывает его срабатывание на этой плате.
[Официальная документация ESP-IDF 5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html#bounce-buffer-with-single-psram-frame-buffer).

Нужны отдельные метрики реального VSYNC, опозданий bounce/восстановлений DMA,
длительности flush и поворота. Не следует включать verbose logging внутри ISR:
измерения сами могут изменить временное поведение. API-снимки существующих
полей этот пробел не закрывают.

### Поле vsync и startup ACK имеют более узкую семантику

В pinned `ESP32_Display_Panel/src/drivers/lcd/esp_panel_lcd.cpp:438` при IDF<5.4
и включённых bounce buffers callback приходит из `on_bounce_frame_finish`,
а не `on_vsync`. Точный драйвер сообщает завершение копирования старого кадра
и выбор источника следующего. Название нашего счётчика `vsync` не делает его
измерением физического сигнала VSYNC или отсутствия дефектов scanout.

Таймаут приложения 250 ms значительно длиннее одного кадра. Его срабатывание
приводит к latched fault и остановке display task. Этот конкретный сценарий
хуже соответствует описанию краткого рывка с немедленным продолжением работы,
но нулевой timeout не исключает более короткого сбоя.

Новый startup ACK подтверждает передачу буфера драйверу, не полный оптический
вывод нового кадра. Baseline после invalidate под mutex и ожидание вне mutex
согласованы. До backlight ON дополнительно проходит минимум 100 ms guarded
quiet, номинально около четырёх кадров. Отдельного нового blocker в этой
последовательности не найдено; оптический тест остаётся обязательным.

Уточнение документации: действующий `src/ui/ui_runtime.c:190` использует
`lv_scr_load`. Generated `src/ui/ui.c`, содержащий fade, исключён из сборки.
Комментарий о необходимом ожидании generated fade описывает другой путь.
Проверка непрозрачного logo безопасна, но наличие fade не следует считать
подтверждённым поведением этой прошивки.

### sdkconfig.defaults не равен настройкам prebuilt SDK

Фактический header:
`C:\Users\user\.platformio\packages\framework-arduinoespressif32-libs\esp32s3\qio_opi\include\sdkconfig.h`.
В нём не заданы `CONFIG_LCD_RGB_ISR_IRAM_SAFE`, `CONFIG_LCD_CAM_ISR_IRAM_SAFE`
и `CONFIG_LCD_RGB_RESTART_IN_VSYNC`. В бинарном драйвере allocation flags
`0x90e` не содержат IRAM bit `0x400`, хотя сами ISR-функции находятся в IRAM.
Строки `...IRAM_SAFE=y` в проектном defaults не доказывают, что эти настройки
применены к prebuilt Arduino/IDF. Автовосстановление bounce, описанное выше,
существует и без настройки принудительного restart на каждом VSYNC.

Это существующее ограничение конфигурационной гарантии, не доказанная причина
рывка. XIP уже включён. Менять SDK или параметры RGB в текущий диагностический
кандидат без отдельного сравнения не следует.

## Проверенные изменения без новой конкретной функциональной находки

- OTA: подтверждение Allow перед сессией; проверка target до Update.begin;
  разделение request/session ID для поздних callbacks; сохранение исходной
  причины отклонения; повторный запрос не получает право записи; restart gate.
- GT911: диагностический 0x5D только в 7-inch окружении; startup, driver и
  recovery используют один адрес. 4.3 остаётся 0x14. Операции, частоты, reset/INT
  и polling последней сменой уровней сообщений не изменены.
- Подсветка: custom callback избегает раннего vendor ON, проверяется результат
  EXIO2, logo должен стать непрозрачным, затем пройти driver handoff. Дальнейшее
  включение использует WakePowerGuard. Обхода guard через web/alarm/schedule в
  просмотренном коде не обнаружено. Темнота до первой firmware-записи CH422G
  и физическая видимость logo всё ещё требуют отдельного наблюдения.
- LVGL lock: ограниченные попытки ожидания logo считаются отдельно; реальные
  неудачи обычных callers не обнуляются и не маскируются. Native-тесты не
  воспроизводят реальное планирование FreeRTOS.
- /diag: старые поля I2C сохранены и обозначены как startup snapshot;
  download выполняет свежие последовательные GET, имеет таймауты, экранирует
  входные строки, помечает partial и не подставляет прежние успешные данные.
- dual-I2C: panel и external sensors разделены по host; backlight продолжает
  использовать существующую сериализацию, не добавлено восстановления шин
  или одновременного изменения аппаратных переменных.

## Локальные проверки

| Проверка | Результат | Ограничение |
| --- | --- | --- |
| Полный canonical native default | 952 PASS | Policy/driver mocks, не реальное железо |
| Отдельный native_test_gt911_5d | 7 PASS | Не cold boot / touch soak |
| Всего native | 959 PASS, 115 environment-suite entries | Проверены actual Unity source paths |
| Python tools/tests | 88 PASS | Изолированный Python с явно добавленным project path |
| JavaScript фактического diag template | 11 PASS | Не скачивание с нового физического BIN |
| EEZ postprocess --check | PASS | Generated UI не менялся |
| git diff --check | PASS с конфигурацией репозитория | См. замечание о CRLF ниже |
| 3 сохранённых свежих firmware build | SUCCESS | Тот же проверенный исходный snapshot, без hardware PASS |

Первый экспериментальный diff-check с принудительным `core.autocrlf=false`
считал CRLF хвостами строк и вернул 2. Проверка без переопределения настроек
репозитория вернула 0. Оба лога сохранены; файлы ради этого не переписывались.
Предупреждения sandbox о недоступном пользовательском git ignore не являются
compiler warning и не скрыты в исходных логах.

## Дальнейшая квалификация

До следующего аппаратного прогона следует исправить F1/F2/F3 локально, сохранить
startup snapshot для F4 и добавить измерения дисплейных событий. Каждый новый
BIN должен иметь отдельные SHA, manifest и запись установки; нынешний 900842ae
не перезаписывать. Оптимизацию редкого опроса тача пока не смешивать с этим.

Для редкого рывка сначала нужен неизменный main-screen сценарий с отметкой
времени/uptime и видео, сохранённой настройкой поворота, счётчиками вывода и
низкоуровневого восстановления. Не менять одновременно PCLK, bounce buffer,
частоты I2C, питание и проводку. Отсутствие WARN не считать оптическим PASS.
Режимы touch/смены страниц и wake проверять отдельно от нетронутого main.

После отдельной авторизации точного BIN: проверить startup logo/backlight,
обычную работу и поворот экрана, release-after-wake, OTA rejection и правильный
OTA, cold boot и software restart раздельно, затем длительный main/touch run.
Не запускать тесты автоматически из этого документа.

Исторические результаты не переносятся: cold-серия старого ff8c6045 завершена
после C04 с capture gap, C05 не выполнен; десять SW restart имеют отдельно
описанные границы наблюдения и не дают десять физических PASS. Отзыв о 13.5 h
относится к 019d87b, raw attachment здесь отсутствует. Он не квалифицирует 0x5D
или нынешний кандидат. CO `--` при неизвестном времени остаётся необъяснённым.

## Evidence

`D:\21cncstudio\project_aura\logs\audit_7_candidate_20260831T205752Z`:
SCOPE.md, BEFORE/AFTER snapshots, native-all.log, native-5d.log, копии native
JSON reports, TEST_RESULTS.json, Python/JS/EEZ/diff-check logs и GET raw JSON.
Архив `display-driver-review` содержит команды, хеши и выдержки реального
дисплейного драйвера/SDK, использованные при ревью.
Прежние BIN/ELF/map, source snapshots и build logs остаются в
`diagnostic_warning_policy_20260831T204001Z`. Root archive и
`dual-profile-release` на `019d87b2bda51d1d6ae9c8d4c967e2cb4af4e8d9` сохранены.

## Статус после исправлений, 2026-09-01

Этот раздел заменяет план «исправить F1-F4» выше фактическим состоянием нового
локального кандидата. Исторические снимки и SHA в предыдущих разделах остаются
записями старых, отдельно собранных BIN.

- F1 исправлен: начальный scanout остаётся на framebuffer 0, LVGL рисует в 1/2;
  при 180° rotated output и renderer не совпадают, ownership меняется только
  после подтверждённого callback. Несогласованное состояние защёлкивает
  display-sync fault и останавливает display task. Добавлены счётчики hand-off,
  wait timeout, rotated copy и ownership violation.
- F2 исправлен: ошибка чтения больше не считается release. После wake gate
  снимается только подтверждённым `Released`; успешный `Pressed` сбрасывает
  error streak, но не открывает новый жест. Probe ограничен обычным интервалом
  12 ms.
- F3 исправлен для recent/alert rings, sequence и API snapshots коротким mutex.
  Полная Serial-строка одного вызова Logger теперь отдельно защищена output
  mutex, без вложенного захвата mutex буферов. Прямые `ESP_LOG`/LVGL `printf`
  остаются за пределами этого lock, а native-тест не доказывает атомарность на
  реальном UART; это ограничения диагностики, а не найденная runtime-регрессия.
- F4 исправлен отдельным bounded `boot.gt911_startup`. Production 7-inch не
  выполняет дополнительные probe reads и честно возвращает `captured=false`;
  diagnostic environment сохраняет configured/alternate results независимо от
  конечного INFO ring.
- Startup race закрыт mutex barrier: LVGL task не может выполнить первый flush
  до успешной регистрации refresh callback. Активный runtime deinit теперь
  fail-closed, потому что vendor callback нельзя безопасно удалить во всех
  вариантах; reboot использует отдельный quiesce path и сохраняет TCB до reset.
- `/api/diag.display.available` теперь определяется lifecycle LVGL, а не полным
  успехом `boot.lvgl_ready`, поэтому startup fail-stop не скрывает счётчики.

Профильная матрица финального исходного дерева:

| Environment | Profile | GT911 | Startup reads | Flip default |
| --- | --- | --- | --- | --- |
| `project_aura` | `4_3` | `0x14` | OFF | OFF |
| `project_aura_7` | `7_dual_i2c_scl6` | `0x5D` | OFF | ON |
| `project_aura_7_gt911_5d` | `7_dual_i2c_scl6` | `0x5D` | ON, `diagnostic_only` | ON |

После этого аудита OTA identity усилена отдельным embedded flavor descriptor.
Production 7-inch принимает только `aura-aq-7-v1` + `production`; diagnostic BIN
использует старо-guard-несовместимый lane target `aura-aq-7-diag-v1` +
`diagnostic`. Поэтому даже предыдущий target-only production guard отклоняет
новый loose diagnostic BIN до записи. Diagnostic firmware принимает свою lane и
production BIN как путь выхода. Новый guard fail-closed отклоняет старые
target-only BIN, поскольку их production/diagnostic происхождение неразличимо.
Подробная матрица и migration limits находятся в
`docs/OTA_HARDWARE_TARGET_GUARD.md`.

Сохранённая пользовательская настройка flip имеет приоритет над profile default.
Частоты шин, питание, GPIO и проводка одновременно не менялись. Оптимизация idle
touch polling остаётся отдельной будущей задачей.

Финальная локальная проверка: canonical native 977/977 в 115 environment-suite
entries, отдельный GT911 0x5D набор 10/10, Python 88/88, фактический `/diag`
JavaScript 11/11, EEZ check PASS. Три финальные сборки не содержат строк compiler
warning/error; OTA target, ESP checksum/hash, RTC ABI и restart-linker проверки
прошли. Первый Python discovery без project path сохранён как invocation error,
затем повторён правильной командой с PASS. Две sandbox-попытки PlatformIO
остановились на доступе к user-profile lock до сборки/теста; успешный повтор
выполнен с нужным доступом. Эти попытки не считаются product test failures.

| Environment | BIN bytes | SHA256 | Hardware state |
| --- | ---: | --- | --- |
| `project_aura` | 4316912 | `0d48aadcea89f02524fcbd8879bd6e613e5a0d46f0f2a16f451e7fcfb50d5bd0` | not flashed |
| `project_aura_7` | 4317408 | `9470262a974a3ee85d2587cf35fda962956f5b5620fb74fb634f012c12c7ed52` | not flashed |
| `project_aura_7_gt911_5d` | 4318608 | `45c1fbcdca82f6abe275122e362a892d5ae853e90ddd7b7672467303cc799c7e` | not flashed |

Эти три hashes относятся к snapshot до добавления OTA flavor descriptor. Они
остаются доказательством только прежних BIN и не являются артефактами новой OTA
границы; после интеграции всех audit fixes необходима новая полная сборка.

Полное evidence:
`D:\21cncstudio\project_aura\logs\post_audit_release_candidate_20260831T234315Z`.
Причина прежнего одиночного вертикального рывка по-прежнему не установлена.
Исправление ownership устраняет подтверждённый программный риск, но требует
холодных стартов и длительного main-screen/touch прогона точного BIN. Контролировать
`display_sync_fault=false`, ownership/timeouts=0, callback gaps и фактический
экран. Нулевые счётчики не заменяют оптическое наблюдение.

Редкий CO `--` разобран отдельно в
`docs/SEN0466_TRANSIENT_INVALID_AUDIT_20260901.md`. Подтверждены два прежних
программных пути краткой invalid state, они закрыты. Причина наблюдавшегося
аппаратного события не установлена; точный новый BIN требует отдельного soak.

## Полная проверка после финального ревью, 2026-09-01

После закрытия замечаний по OTA diagnostic boundary, canonical 7-inch coverage,
Logger и границе прогрева CO выполнена новая проверка текущего dirty snapshot.
Warmup grace ограничен 18 секундами, optional-gas step 0.01 учитывает float
погрешность, а `/api/charts` получает type, count, epoch, entries и derived
latest из одной heap-owned immutable snapshot одного поколения.
Исторические hashes выше сохранены как данные прежних кандидатов.

- canonical native launcher: 996/996 PASS в 117 environment-suite entries,
  включая `native_test_gt911_5d` в штатном наборе;
- Python tools: 94/94 PASS;
- фактический JavaScript `/diag`: 11/11 PASS;
- EEZ postprocess check и `git diff --check`: PASS;
- `project_aura`, `project_aura_7` и `project_aura_7_gt911_5d`: BUILD PASS;
- для всех трёх BIN прошли embedded target/flavor, ESP checksum/hash, RTC ABI и
  restart-linker проверки.

| Environment | Bytes | SHA256 | Embedded identity | Hardware state |
| --- | ---: | --- | --- | --- |
| `project_aura` | 4318640 | `2393ec0621ca9eb8b16dbc257a349fad5830a788de29c783b25d3680425b64d6` | `aura-aq-v1` + `production` | not flashed |
| `project_aura_7` | 4319344 | `8d8e3744892bd2ab90fbd2d74502c7d4529a6f329714522f20f8b6d23dffb189` | `aura-aq-7-v1` + `production` | not flashed |
| `project_aura_7_gt911_5d` | 4320352 | `3b664e20baf2158c584946d355fc460c6edffb612f4abadee4f739924bb9343d` | `aura-aq-7-diag-v1` + `diagnostic` | not flashed |

Перед чистым полным PASS одна попытка остановилась до запуска NTP suite из-за
краткого Windows file lock на native `program.exe`. Изолированный NTP повтор
прошёл 9/9, соседняя TimeManager плюс NTP последовательность прошла 51/51, после
чего новый canonical run прошёл 996/996. Это build-infrastructure transient, а
не переносимый PASS или failure прошивки.

Evidence: `D:\21cncstudio\project_aura\logs\post_review_fixes_candidate_20260901T090555Z`.
Никакие платы, COM, OTA, reset, release packages или remote refs этой проверкой
не затрагивались. Причины редкого вертикального сдвига и наблюдавшегося CO `--`
не объявляются установленными до аппаратной проверки именно этих BIN.
