# Навык: отработка типовых ошибок сборки и терминала ESP-IDF + LVGL

## 0. Золотое правило: СНАЧАЛА посмотреть примеры, ПОТОМ писать код
Прежде чем писать или править любой код в ESP-IDF/LVGL проекте, обязательно изучить связанные
с ним примеры — это экономит часы на отладке типовых ошибок из этого навыка (почти все разделы 1–12
возникли именно из-за кода «по памяти», без сверки с примерами).

Порядок действий перед написанием кода:
1. **Официальные примеры ESP-IDF** (`$IDF_PATH/examples/`) — искать по подсистеме:
   - Wi-Fi SoftAP + HTTP-сервер: `examples/wifi/getting_started/softAP`, `examples/protocols/http_server/restful_server`
   - DNS/сокеты: `examples/protocols/sockets/udp_client` (набор инклюдов BSD-сокетов — раздел 11)
   - UART/консоль: `examples/peripherals/uart/uart_echo`, `examples/system/console`
   - LVGL/дисплеи: managed-компонент `esp_lvgl_port` содержит `test_apps/` и README с эталонным кодом инициализации.
2. **mdns** больше не в IDF — пример в репозитории `espressif/esp-idf-mdns` (папка `examples/`).
3. **Заголовки своей версии IDF** — проверять фактическое существование символов:
   `grep -rn "WIFI_EVENT_AP_START" $IDF_PATH/components/esp_wifi/include/` — никогда не писать
   имена констант/функций по памяти (ошибки ESP_ERR_NOT_FIT, WIFI_AP_STARTUP, WIFI_EVENT_AP_STARTUP — разделы 3, 11).
4. **Свой же проект** — посмотреть, как аналогичная задача решена в других ветках/файлах
   (`git grep socket\(`, `git log --oneline -- main/softap.c`), чтобы не изобрести сломанное заново.
5. **Совместимость версий** — при переходе на новую мажорную IDF сверяться с
   migration guide (`docs.espressif.com → migration-guides/release-6.x/`): разбивка `driver` на
   `esp_driver_*` (раздел 8), перенос mdns (раздел 9) описаны там официально.

Только после изучения примера писать код, повторяя его структуру инклюдов, имён констант и зависимостей CMakeLists.

## 1. fatal error: lvgl/lvgl.h: No such file or directory
Причина: сгенерированные lv_font_conv шрифты делают `#include "lvgl/lvgl.h"`, а в сборке ESP-IDF заголовок лежит как `lvgl.h` (компонент добавляет свой include-путь).
Решение (в трёх слоях):
- `main/CMakeLists.txt`: `target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)`
- `sdkconfig.defaults`: `CONFIG_LV_LVGL_H_INCLUDE_SIMPLE=y`
- в своих .c файлах guarded include с резервным путём.

## 2. undefined reference to `font_xxx` при линковке (.elf FAILED)
Причина: файлы шрифтов либо не компилируются вообще, либо подключены `#include` в другой .c И одновременно в SRCS (дубли определений), либо только через #include с удалённым из SRCS файлом.
Решение: добавить все `assets/fonts/*.c` в `SRCS` и вместо include использовать `LV_FONT_DECLARE(...)`.

## 3. error: 'ESP_ERR_NOT_FIT' undeclared
Причина: несуществующая константа esp_err.h.
Решение: использовать реальные коды — `ESP_ERR_INVALID_SIZE`, `ESP_ERR_NOT_FOUND`, `ESP_FAIL`. Проверять список в `esp_err.h`.

## 4. IRAM conflict / section attribute conflicts
Причина: функция помечена `IRAM_ATTR`, но вызываемый код не в IRAM (или наоборот, CONFIG_SPI_MASTER_IN_IRAM конфликтует).
Решение: убрать атрибут или включить соответствующий CONFIG_*_IN_IRAM.

## 5. Переполнение flash (.rodata too large) после добавления шрифтов
Причина: кириллические диапазоны lv_font_conv дают ~1 МБ на 4 шрифта.
Решение: сузить диапазоны глифов, `--compress`, либо `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`; проверять остаток места (`check_flash_space()` / `idf.py size`).

## 6. Частота/меню не меняется через терминал (ввод игнорируется)
Симптомы: меню печатается, но любой ввод приводит к значению по умолчанию.
Причины и решения (по порядку проверки):
1. **stdin не привязан к UART**: в IDF v5+ нужен `CONFIG_ESP_CONSOLE_UART_DEFAULT=y` + `CONFIG_ESP_CONSOLE_UART_SECONDARY_INPUT=y` (иначе secondary = NONE и fgets возвращает EOF мгновенно).
2. **fgets без таймаута зависает/возвращает EOF** — надёжнее принять байты напрямую: `uart_driver_install(UART_NUM_0,...)` + `uart_read_bytes()` (см. `main/uart_input.c`), опционально `esp_vfs_dev_uart_use_driver(0)` для stdin.
3. **Не тот порт/скорость**: терминал должен быть на UART0, 115200 8N1; на C3 Super Mini USB-C — это CDC (USB_SERIAL_JTAG), тогда нужен `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` и ввод идёт через него (порт 0 в driver/uart = true UART0, не USB!).
4. **Консоль «съедает» символы**: ROM-загрузчик и log-поток делят UART0; если драйвер уже установлен консолью, повторный install вернёт ESP_ERR_INVALID_STATE — это штатно, читать можно.
5. **Прошивка не пересобрана**: sdkconfig.defaults применяется только при УДАЛЁННОМ sdkconfig — сделать `idf.py fullclean` + удалить `sdkconfig`, затем build.

## 8. fatal error: driver/uart.h: No such file or directory (ESP-IDF 6.x)
Симптом:
```
main/uart_input.c:19:10: fatal error: driver/uart.h: No such file or directory
```
Причина: в ESP-IDF 6.x монолитный компонент `driver` разбит на подкомпоненты
(`esp_driver_uart`, `esp_driver_gpio`, `esp_driver_spi`, ...). Заголовок
`driver/uart.h` физически живёт в `esp_driver_uart`, и одного `REQUIRES driver`
больше недостаточно — include-путь подкомпонента не пробрасывается.
Решение: в `main/CMakeLists.txt` перечислять конкретные драйвер-компоненты:
```cmake
REQUIRES      esp_driver_gpio esp_driver_spi esp_driver_uart
              esp_hw_support esp_timer vfs
PRIV_REQUIRES esp_lcd esp_partition app_update esp_wifi esp_netif esp_event
```
Правило: при переходе на IDF 6.x все зависимости вида `driver/xxx.h` заменять на
соответствующий `esp_driver_xxx`.

Дополнительно (уточнение по API v6): классический драйвер UART (`uart_driver_install`,
`uart_config_t`, `uart_read_bytes`) в IDF 6.x перенесён в `driver/uart_v1.h` — старый
`driver/uart.h` работает как совместимый алиас, но если компилятор его не находит,
используйте guarded include:
```c
#if __has_include("driver/uart.h")
#include "driver/uart.h"        // IDF 5.x / совместимый путь 6.x
#else
#include "driver/uart_v1.h"     // IDF 6.x: классический UART-драйвер
#endif
```
Рекомендуемый паттерн CMakeLists (работает и на 5.x, и на 6.x без ручных правок):
```cmake
set(MAIN_REQUIRES esp_driver_uart esp_driver_gpio esp_driver_spi ...)
if(NOT EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    list(APPEND MAIN_REQUIRES driver)   # ESP-IDF <= 5.x: зонтичный компонент
endif()
```

## 9. CMake Error: Failed to resolve component 'mdns' ... unknown name (ветка softap-webui)
Причина: mDNS удалён из дерева ESP-IDF 6.x и распространяется как managed-компонент.
Решение:
1. `main/idf_component.yml`:
   ```yaml
   dependencies:
     espressif/mdns: "^1.2.0"
   ```
2. В `main/CMakeLists.txt` разрешать имя динамически (`espressif__mdns` для 6.x,
   `mdns` для 5.x). ВАЖНО: managed-компоненты лежат в `<корень проекта>/managed_components`,
   а НЕ рядом с `main/`; проверять нужно оба расположения плюс дерево IDF, иначе
   EXISTS всегда false и в REQUIRES попадает голое `mdns` → та же ошибка.
3. После изменений обязательно `idf.py fullclean` (stale build-дир кэширует старый список компонентов).

## 10. Страница captive portal не открывается автоматически на телефоне
(актуально для ветки softap-webui, ESP32C3-DGW-SA1)
Причины и решения:
- Android/iOS проверяют connectivity-эндпоинты; обычный HTTP 302 часто игнорируется.
  Нужен HTML-ответ с `<meta http-equiv="refresh">` на страницу портала;
- DNS-сервер на порту 53 должен отвечать на ЛЮБОЙ запрос адресом AP (192.168.4.1),
  сокет создавать с `SO_REUSEADDR`;
- открыть системный диалог «Добавить сеть Wi-Fi» через Intent можно только с
  разрешения пользователя — автозапуск страницы ограничивается подсказкой в SSID
  и редиректом после подключения.

## 11. Ошибки BSD-сокетов и Wi-Fi событий в softap.c (ESP-IDF 6.x)
Симптом (одна сборка, много ошибок):
```
error: 'WIFI_AP_STARTUP' undeclared
error: storage size of 'src' isn't known          (struct sockaddr_in)
error: unknown type name 'socklen_t'
error: implicit declaration of function 'recvfrom'/'sendto'/'socket'/'bind'/'setsockopt'
error: 'AF_INET'/'SOCK_DGRAM'/'INADDR_ANY' undeclared
error: implicit declaration of function 'ESP_RETURN_ON_ERROR'
```
Причины:
1. В IDF 6.x сетевые заголовки **не подключаются транзитивно** через `esp_wifi.h`/`lwip` — файл,
   использующий сокеты, обязан включать их сам;
2. использовалось **выдуманное имя события**: ни `WIFI_AP_STARTUP`, ни `WIFI_EVENT_AP_STARTUP`
   не существует НИ в IDF 5.x, НИ в 6.x. Правильное имя — `WIFI_EVENT_AP_START`
   (проверено по исходникам v5.4 и v6.0: enum `wifi_event_t`, `esp_wifi_types_generic.h`).
   Компилятор прямо подсказывает его: `did you mean 'WIFI_EVENT_AP_START'?`;
3. `ESP_RETURN_ON_ERROR` живёт в `esp_check.h`, который тоже надо включить явно.

Решение (полный набор инклюдов для любого socket-кода):
```c
#include <unistd.h>           // close
#include <sys/socket.h>       // socket, bind, recvfrom, sendto, setsockopt, socklen_t, SOL_SOCKET, SO_REUSEADDR
#include <sys/types.h>        // ssize_t
#include <netinet/in.h>       // sockaddr_in, AF_INET, IPPROTO_UDP, htons/htonl, INADDR_ANY
#include <arpa/inet.h>
#include "esp_check.h"        // ESP_RETURN_ON_ERROR
```
Правильное использование имени события (ВАЖНО — типичная ошибка №2):
```c
if (id == WIFI_EVENT_AP_START) { ... }   // единственно верное имя во всех IDF 5.x/6.x
```
НЕЛЬЗЯ оборачивать enum-константы в `#if defined(...)`: значения enum — это НЕ макросы,
`defined(WIFI_EVENT_AP_START)` всегда даёт ложь, и компилятор уходит в `#else`-ветку
с несуществующим именем. Ровно так и возникал повторный `'WIFI_AP_STARTUP' undeclared`
в мёртвой ветке препроцессора. Правило: для enum-констант IDF использовать имя напрямую,
проверки совместимости допустимы только через `__has_include` (заголовки) или
`ESP_IDF_VERSION_MAJOR/MINOR` (`esp_idf_version.h`).
ГЛАВНОЕ ПРАВИЛО ИСПРАВЛЕНИЙ: перед заменой «неизвестного» имени на «правильное» —
проверить подсказку компилятора (`did you mean ...`) и найти символ в реальных заголовках
IDF (github.com/espressif/esp-idf → поиск по тегу версии). Предыдущая «фикса»
`WIFI_AP_STARTUP → WIFI_EVENT_AP_STARTUP` сама была выдуманным именем и породила
повторную ошибку — подмена без проверки по исходникам запрещена.
Плюс порядок инклюдов в TU с LVGL: системные сетевые заголовки размещать ДО lvgl-заголовков
(в IDF 6.x lvgl.h может тянуть lwip-обёртки, конфликтующие с `<unistd.h>` при обратном порядке).

---

## MCP-серверы Espressif: использование в работе над проектом

### Статус проверки (октябрь 2026)
В текущем рабочем окружении (isolated Linux-контейнер без ESP-IDF и без последовательных портов)
MCP-серверы Espressif **не используются**: они требуют локальной установки IDF
(`C:/esp/v6.0.3/esp-idf`) и физического устройства. Проверено: pip-пакетов
`espressif-mcp`, `esp-idf-mcp-server`, `idf-mcp` в PyPI нет — сервер распространяется иначе.

### Что доступно официально от Espressif
1. **`idf_claude_skill`** (https://github.com/espressif/idf_claude_skill) — официальный репозиторий
   навыков+MCP для ESP-IDF v6.x. Устанавливается как skill-пакет в Claude Code / совместимые агенты:
   ```bash
   git clone https://github.com/espressif/idf_claude_skill.git
   cd idf_claude_skill && ./install.sh --path <каталог-агента>
   # или вручную: скопировать skills/ в ~/.claude/skills, добавить MCP-сервер в конфиг агента
   ```
   После установки агент получает инструменты: разбор ошибок сборки idf.py, миграция 5.x→6.x,
   поиск по API-докам, анализ размера прошивки.
2. **MCP-сервер idf.py** (встроен в IDF v6.x начиная с 6.0):
   ```bash
   idf.py mcp server            # stdio-транспорт
   # в .mcp.json клиента:
   { "mcpServers": { "esp-idf": { "command": "idf.py", "args": ["mcp", "server"] } } }
   ```
   Даёт агенту инструменты build/flash/monitor, чтение логов, dfu, eFuse и пр. Работает только там,
   где установлена IDF и виден COM-порт.
3. **ESP-MCP (community)** — сторонние обёртки над idf.py/pyserial; использовать только если
   официальные недоступны.

### Как применять в этом проекте (рекомендация)
- На машине разработчика (Windows, `C:\Espressif`, IDF 6.0.3) подключить `idf.py mcp server`
  к IDE-агенту → агент сам запускает `build`, читает полный лог ошибки и сверяется с этим навыком;
- Логику «упал build → открыть ERRORS.md → найти номер раздела → применить решение» этот навык
  полностью покрывает и без MCP (см. разделы 1–12), поэтому MCP — ускорение, а не зависимость;
- При обновлении IDF проверять `idf.py --version` и changelog миграции:
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32/migration-guides/release-6.x/

---

## 12. CMake Error: Failed to resolve component 'esp_vfs_dev' ... unknown name
Симптом: сборка падает ЕЩЁ до компиляции, на этапе `Processing dependencies`:
```
CMake Error at .../tools/cmake/build.cmake (message):
  Failed to resolve component 'esp_vfs_dev' required by component 'main': unknown name.
```
Причина: `esp_vfs_dev` — это НЕ компонент ESP-IDF, а всего лишь заголовок/модуль
внутри компонента `vfs`. Указывать его в `REQUIRES` нельзя.
Решение:
- в `main/CMakeLists.txt` заменить `esp_vfs_dev` на `vfs` (для IDF 5.x);
- в IDF 6.0 модуль esp_vfs_dev упразднён полностью, а `esp_vfs_dev_uart_use_driver()`
  перенесена в `driver/uart_vfs.h` (компонент `driver`) — в REQUIRES достаточно `driver`;
- для совместимости 5.x/6.x использовать guarded include:
```c
#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"   // ESP-IDF 6.x
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"       // ESP-IDF 5.x
#endif
```
Правило: в REQUIRES перечислять только реальные компоненты (`ls $IDF_PATH/components`).

## Чек-лист перед push
- [ ] **Посмотрены связанные примеры (раздел 0) ДО написания кода**
- [ ] `idf.py build` проходит локально (или CI зелёный)
- [ ] новые .c добавлены в SRCS, зависимости — в REQUIRES
- [ ] CONFIG-флаги, влияющие на ввод/консоль, внесены в sdkconfig.defaults
- [ ] socket-код содержит полный набор инклюдов (раздел 11)
- [ ] комментарии на русском, сообщения лога — на русском
- [ ] свежая ошибка добавлена в ERRORS.md и в этот навык
