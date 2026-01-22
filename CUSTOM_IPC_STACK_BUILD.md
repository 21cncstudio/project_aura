# Инструкция по сборке Arduino ESP32 с увеличенным IPC Stack

## Проблема
Arduino ESP32 использует предкомпилированные библиотеки ESP-IDF, где `CONFIG_ESP_IPC_TASK_STACK_SIZE=1024` байта зашит жестко. Это вызывает stack overflow при инициализации некоторых драйверов (GT911, GPIO и т.д.).

## Решение
Собрать собственную версию Arduino ESP32 framework с увеличенным IPC stack size.

---

## Вариант 1: Использование esp32-arduino-lib-builder (Рекомендуется)

### Шаг 1: Подготовка окружения

1. **Установите зависимости:**
   - Python 3.8+
   - Git
   - CMake 3.16+
   - Ninja build

   В Windows (через PowerShell от администратора):
   ```powershell
   # Установите Chocolatey если еще не установлен
   Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

   # Установите зависимости
   choco install git cmake ninja python
   ```

2. **Клонируйте esp32-arduino-lib-builder:**
   ```bash
   cd C:\esp32-build
   git clone https://github.com/espressif/esp32-arduino-lib-builder
   cd esp32-arduino-lib-builder
   ```

### Шаг 2: Настройка IPC Stack Size

1. **Создайте файл конфигурации:**
   ```bash
   # В директории esp32-arduino-lib-builder создайте configs/defconfig.common
   mkdir -p configs
   ```

2. **Создайте файл `configs/defconfig.common`** со следующим содержимым:
   ```
   CONFIG_ESP_IPC_TASK_STACK_SIZE=4096
   CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
   CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=4096
   ```

3. **Или отредактируйте существующие файлы defconfig:**
   ```bash
   # Найдите все defconfig файлы
   find . -name "defconfig*" -type f

   # Добавьте в каждый найденный файл:
   echo "CONFIG_ESP_IPC_TASK_STACK_SIZE=4096" >> ./configs/defconfig.esp32s3
   ```

### Шаг 3: Сборка библиотек

1. **Запустите сборку для ESP32-S3:**
   ```bash
   ./build.sh -t esp32s3 -b 3.1.1
   ```

   Где:
   - `-t esp32s3` - целевая платформа (ваш чип)
   - `-b 3.1.1` - версия Arduino ESP32

   **ВНИМАНИЕ:** Сборка может занять 1-3 часа!

2. **Дождитесь завершения.** Результат будет в папке:
   ```
   out/arduino-esp32/
   ```

### Шаг 4: Интеграция в PlatformIO

1. **Скопируйте собранные библиотеки:**
   ```bash
   # Найдите путь к Arduino ESP32 framework в PlatformIO
   # Обычно: C:\Users\<USER>\.platformio\packages\framework-arduinoespressif32\

   # Создайте бэкап оригинальных библиотек
   cd C:\Users\vpapu\.platformio\packages\framework-arduinoespressif32
   mkdir backup
   xcopy /E /I tools\sdk backup\tools_sdk

   # Скопируйте новые библиотеки
   xcopy /E /Y C:\esp32-build\esp32-arduino-lib-builder\out\tools\sdk\esp32s3\* tools\sdk\esp32s3\
   ```

2. **Очистите проект и пересоберите:**
   ```bash
   cd "d:\CNCmy\8 My laser\Bambulab\esp32-improved-weather-station\esp32 43 new\platformio\project aura"
   rmdir /s .pio
   pio run
   ```

---

## Вариант 2: Патчинг через menuconfig (Быстрее, но сложнее)

### Требования:
- Linux или WSL (Windows Subsystem for Linux)
- ESP-IDF установлен

### Шаги:

1. **Установите ESP-IDF 5.3.x:**
   ```bash
   mkdir -p ~/esp
   cd ~/esp
   git clone --recursive https://github.com/espressif/esp-idf.git -b v5.3.1
   cd esp-idf
   ./install.sh esp32s3
   . ./export.sh
   ```

2. **Клонируйте Arduino ESP32:**
   ```bash
   cd ~/esp
   git clone https://github.com/espressif/arduino-esp32.git -b 3.1.1
   cd arduino-esp32
   ```

3. **Запустите menuconfig:**
   ```bash
   idf.py menuconfig
   ```

4. **Измените настройки:**
   - Перейдите в: `Component config` → `ESP System Settings`
   - Найдите: `IPC task stack size`
   - Измените с `1024` на `4096`
   - Сохраните (S) и выйдите (Q)

5. **Соберите библиотеки:**
   ```bash
   cd ~/esp/arduino-esp32
   git submodule update --init --recursive
   cd tools
   python get.py
   python build_boards.py esp32s3
   ```

6. **Упакуйте и скопируйте в PlatformIO:**
   ```bash
   # Создайте архив
   cd ~/esp/arduino-esp32/tools/sdk/esp32s3
   tar -czf ~/custom-esp32s3-libs.tar.gz *

   # Скопируйте в Windows и распакуйте в:
   # C:\Users\vpapu\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32s3\
   ```

---

## Вариант 3: Использование готовой сборки (Самый быстрый, если найдёте)

Поищите готовые сборки с увеличенным IPC stack на:
- GitHub releases esp32-arduino-lib-builder
- Форумах ESP32
- Community builds

---

## Вариант 4: Использование platform_packages с локальным путём

Если вы собрали собственную версию Arduino ESP32, укажите её в `platformio.ini`:

```ini
[env:project_aura]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.11/platform-espressif32.zip
platform_packages =
    framework-arduinoespressif32 @ file://C:/esp32-build/arduino-esp32
    platformio/framework-arduinoespressif32-libs @ file://C:/esp32-build/custom-libs.zip
```

---

## Проверка результата

После сборки и установки, в логах при загрузке вы должны увидеть:

```
[I][Main] IPC Task Stack Size: 4096 bytes
```

Вместо:
```
[I][Main] IPC Task Stack Size: 1024 bytes
```

И устройство должно загружаться без ошибки:
```
Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (ipc1)
```

---

## Альтернативное решение без пересборки

Если сборка слишком сложна, можно попробовать:

1. **Использовать более старую версию библиотек**, где проблемы может не быть
2. **Отключить проблемные функции** (например, тачскрин)
3. **Переписать драйвер GT911** для уменьшения использования стека

---

## Полезные ссылки

- [esp32-arduino-lib-builder](https://github.com/espressif/esp32-arduino-lib-builder)
- [Arduino ESP32 GitHub](https://github.com/espressif/arduino-esp32)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [PlatformIO Custom Platforms](https://docs.platformio.org/en/latest/platforms/creating_platform.html)

---

## Поддержка

Если возникнут проблемы при сборке:
1. Проверьте версии зависимостей
2. Убедитесь, что используете правильную ветку ESP-IDF
3. Проверьте логи сборки на ошибки
4. Обратитесь на форум ESP32: https://esp32.com/

---

**Примечание:** Процесс сборки сложный и может занять несколько часов. Убедитесь, что у вас достаточно свободного места на диске (минимум 20 ГБ) и стабильное интернет-соединение.
