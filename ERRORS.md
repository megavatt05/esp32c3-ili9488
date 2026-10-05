# Журнал ошибок сборки и отладки — esp32c3-ili9488 (ESP-IDF 6.0.3, ESP32-C3 SuperMini + ILI9488)

Сводный документ по **всем** ошибкам, встреченным в ходе разработки в ветках
`main`, `lvgl9` и `softap-webui`. Каждая запись: симптом → причина → решение → статус.

| № | Ошибка | Ветка | Статус |
|---|--------|-------|--------|
| 1 | `undefined reference to font_*` при линковке .elf | lvgl9 | ✅ исправлено |
| 2 | `fatal error: lvgl/lvgl.h: No such file or directory` | lvgl9 | ✅ исправлено |
| 3 | `'ESP_ERR_NOT_FIT' undeclared` | main | ✅ исправлено |
| 4 | Частота SPI не меняется через терминал | main | ✅ исправлено |
| 5 | `Failed to resolve component 'esp_vfs_dev': unknown name` | main | ✅ исправлено |
| 6 | `fatal error: driver/uart.h: No such file or directory` | main | ✅ исправлено |
| 7 | `Failed to resolve component 'mdns': unknown name` | softap-webui | ✅ исправлено |
| 8 | Переполнение flash из-за кириллических шрифтов (~1 МБ) | lvgl9 | ⚠️ профилактика |
| 9 | IRAM conflict (`ignoring attribute section`) | lvgl9 | ⚠️ профилактика |
| 10 | Captive portal не открывается автоматически на телефоне | softap-webui | ✅ исправлено |
| 11 | `WIFI_AP_STARTUP` undeclared + BSD-сокеты не объявлены в softap.c | softap-webui | ✅ исправлено |

---

## 1. `undefined reference to font_inter_16 / font_roboto_20 / ...` (FAILED: .elf)
**Ветка:** lvgl9 · **Коммит:** `9ae6caf`

**Симптом:** сборка доходит до `[4/6] Linking CXX executable esp32c3-ili9488.elf` и падает — линкер не находит символы шрифтов LVGL.

**Причина:** файлы шрифтов `assets/fonts/*.c` были исключены из `SRCS` компонента `main`, при этом `#include` этих файлов из `ui.c` удалён — определения шрифтов не компилировались вообще. Вариант-ловушка: одновременно `#include` в `ui.c` **и** наличие в `SRCS` даёт duplicate definition.

**Решение:**
- все четыре `assets/fonts/font_*.c` добавлены в `SRCS` в `main/CMakeLists.txt`;
- в `ui.c` только объявления `LV_FONT_DECLARE(font_xxx)` + макросы `FONT_BODY/FONT_VALUE/FONT_BIG/FONT_TITLE`;
- никаких `#include` *.c-файлов шрифтов.

---

## 2. `fatal error: lvgl/lvgl.h: No such file or directory` (font_notosans_28.c:10)
**Ветка:** lvgl9 · **Коммит:** `28df509`

**Причина:** генератор `lv_font_conv` пишет в начало файла `#include "lvgl/lvgl.h"`. В managed-компоненте `lvgl__lvgl` include-путь указывает на `src/`, где заголовок лежит как `lvgl.h` — путь `lvgl/lvgl.h` не резолвится.

**Решение (три слоя):**
1. `target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)` в `main/CMakeLists.txt`;
2. `CONFIG_LV_LVGL_H_INCLUDE_SIMPLE=y` в `sdkconfig.defaults`;
3. guarded include с резервным путём в собственных файлах:
```c
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
```

---

## 3. `error: 'ESP_ERR_NOT_FIT' undeclared; did you mean 'ESP_ERR_NOT_FOUND'?`
**Ветка:** main · **Коммит:** `b027d86`

**Причина:** `ESP_ERR_NOT_FIT` — несуществующая константа; её нет в `esp_err.h`.

**Решение:** заменить на реальную `ESP_ERR_INVALID_SIZE`. Правило: перед использованием кода ошибки сверяться со списком в `components/esp_common/include/esp_err.h`.

---

## 4. Меню частоты SPI не реагирует на ввод через терминал
**Ветка:** main · **Коммиты:** `657eb82`, инструкция в навыке (п.6)

**Симптом:** меню печатается, но любой ввод приводит к значению по умолчанию (40 МГц).

**Причины (по порядку):**
1. В IDF v5+ stdin по умолчанию НЕ привязан к UART-консоли — `fgets()` мгновенно возвращает EOF;
2. `fgets` без таймаута зависает при неподключённом терминале;
3. на C3 SuperMini USB-C порт — это USB-Serial-JTAG, а не UART0: нужен `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`;
4. `sdkconfig.defaults` применяется только при **удалённом** файле `sdkconfig`.

**Решение:** написан `main/uart_input.c` — надёжный опрос UART0 напрямую через `uart_read_bytes()` с таймаутом 30 с; для stdin вызывается `esp_vfs_dev_uart_use_driver(0)`; добавлен прямой ввод МГц (например `45M`) кроме нумерованного списка 1–8.

---

## 5. `CMake Error: Failed to resolve component 'esp_vfs_dev' required by component 'main': unknown name`
**Ветка:** main · **Коммит:** `a33fd07`

**Причина:** `esp_vfs_dev` — не компонент, а модуль внутри компонента `vfs` (IDF ≤5.3); в IDF 6.0 упразднён полностью, функция перенесена в `driver/uart_vfs.h`. Указывать заголовки/модули в `REQUIRES` нельзя.

**Решение:** в `REQUIRES` — только реальные компоненты (`ls $IDF_PATH/components`); guarded include для совместимости версий:
```c
#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"   // ESP-IDF 6.x
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"       // ESP-IDF 5.x
#endif
```

---

## 6. `fatal error: driver/uart.h: No such file or directory` (uart_input.c:19)
**Ветка:** main · **Коммит:** `4062f51`

**Причина:** в ESP-IDF 6.x монолитный компонент `driver` разбит на подкомпоненты (`esp_driver_uart`, `esp_driver_gpio`, `esp_driver_spi`, …). Заголовок `driver/uart.h` живёт в `esp_driver_uart`, и одного `REQUIRES driver` недостаточно — include-путь не пробрасывается.

**Решение:** в `main/CMakeLists.txt` перечислять конкретные драйвер-компоненты:
```cmake
set(MAIN_REQUIRES esp_driver_uart esp_driver_gpio esp_driver_spi ...)
if(NOT EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    list(APPEND MAIN_REQUIRES driver)   # ESP-IDF <= 5.x: зонтичный компонент
endif()
```
Правило миграции на 6.x: `driver/xxx.h` → зависимость `esp_driver_xxx`.

**Уточнение (официальная миграция 6.x):** классический UART-драйвер (`uart_driver_install`,
`uart_config_t`) в IDF 6.x перенесён в `driver/uart_v1.h`; `driver/uart.h` остаётся
совместимым алиасом при корректной зависимости от `esp_driver_uart`. Если алиас не
найдётся — guarded include `#if __has_include("driver/uart.h") ... #else #include "driver/uart_v1.h"`.

**Обновление:** исправление продублировано во ВСЕ ветки — `main` (`7d082c6`),
`lvgl9` (`d71a7d0`), `softap-webui` (`2d9b166`). В lvgl9/softap-webui UART не
используется, поэтому там достаточно `esp_driver_gpio` + `esp_driver_spi`.

---

## 7. `CMake Error: Failed to resolve component 'mdns' required by component 'main': unknown name`
**Ветка:** softap-webui · **Коммиты:** `f8b1159`, `b047fe3`

**Причина:** mDNS удалён из дерева ESP-IDF 6.x и распространяется как отдельный managed-компонент `espressif/mdns`. Кроме того, после установки имя компонента — `espressif__mdns` (с префиксом), а managed-компоненты лежат в `<корень проекта>/managed_components`, а НЕ рядом с `main/`.

**Решение:**
1. `main/idf_component.yml`:
   ```yaml
   dependencies:
     espressif/mdns: "^1.2.0"
   ```
2. Динамическое разрешение имени в `main/CMakeLists.txt` (перебор `PROJECT_DIR/managed_components`, `main/../managed_components`, `$ENV{IDF_PATH}/components`; приоритет `espressif__mdns`, fallback `mdns`);
3. Обязательный `idf.py fullclean` после изменений — stale build-дир кэширует старый список компонентов.

---

## 8. Переполнение flash из-за шрифтов (профилактика)
**Ветки:** lvgl9, softap-webui

4 кириллических шрифта, сгенерированных `lv_font_conv`, занимают ~1 МБ `.rodata` — на модуле 4 МБ с OTA-разделами это критично.

**Меры:** минимальные диапазоны глифов, флаг `--compress`, `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`; контроль остатка места функцией `check_flash_space()` (сканирование пустых областей 0xFF с конца app-раздела, предупреждение при <5%) и `idf.py size`.

---

## 9. IRAM conflict (профилактика)
**Ветки:** lvgl9

**Симптом:** `ignoring attribute 'section (".iram1.xx")' because it conflicts with previous ...` — конфликт `LV_ATTRIBUTE_FAST_MEM_USE_IRAM` с новыми GCC/IDF.

**Мера:** `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=n` в `sdkconfig.defaults`.

---

## 10. Страница портала не открывается автоматически на телефоне
**Ветка:** softap-webui · **Коммит:** `f8b1159`

**Причины:** Android/iOS проверяют connectivity-эндпоинты и игнорируют обычный HTTP 302; DNS-сервер должен отвечать на ЛЮБОЙ запрос адресом AP.

**Решение:** HTML-ответ с `<meta http-equiv="refresh">` вместо 302; DNS:53 с `SO_REUSEADDR`, отвечающий 192.168.4.1 на все запросы; SSID `DGW-SA1-<XXXX>` + пароль `displaygw`; дополнительно доступен по `http://dgw-sa1.local`.

---

---

## Инструментарий: MCP-серверы Espressif

В данном окружении разработки MCP-серверы Espressif **не подключены и не используются**
(нет локальной ESP-IDF и COM-портов). Проверено: pip-пакетов `espressif-mcp`,
`esp-idf-mcp-server`, `idf-mcp` в PyPI не существует.

Что можно подключить на машине разработчика (Windows + IDF 6.0.3):
1. **Встроенный MCP-сервер idf.py** (IDF ≥ 6.0): `idf.py mcp server` (stdio), конфиг клиента:
   ```json
   { "mcpServers": { "esp-idf": { "command": "idf.py", "args": ["mcp", "server"] } } }
   ```
2. **Официальный пакет навыков**: https://github.com/espressif/idf_claude_skill
   (разбор ошибок сборки, миграция 5.x→6.x, анализ размера прошивки).

Порядок применения при падении сборки: лог ошибки → таблица выше → соответствующий раздел
навыка `skills/esp-idf-build-error-handling/SKILL.md`. Подробное описание MCP — там же.

---

## Общие правила (чек-лист перед push)
- [ ] `idf.py build` проходит локально (или CI зелёный) ДО пуша;
- [ ] после правки `REQUIRES`/`sdkconfig.defaults` — `idf.py fullclean` (+ удалить `sdkconfig`, если менялись defaults);
- [ ] в `REQUIRES` только реальные компоненты; для IDF 6.x — `esp_driver_*` вместо `driver`;
- [ ] socket-код: полный набор инклюдов `<sys/socket.h>/<netinet/in.h>/<unistd.h>` + `esp_check.h` (см. п.11);
- [ ] новые .c добавлены в `SRCS`, управляемые зависимости — в `main/idf_component.yml`;
- [ ] `.gitignore` содержит `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock`;
- [ ] комментарии и сообщения лога — на русском;
- [ ] навыки отработки ошибок обновлены: `skills/esp-idf-build-error-handling/SKILL.md` (main), `skills/lvgl-esp-idf-error-handling/SKILL.md` (lvgl9, softap-webui).

*Документ актуален на коммит `4062f51` (main) / `b047fe3` (softap-webui) / `28df509` (lvgl9).*
