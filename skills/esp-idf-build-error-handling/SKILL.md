# Навык: отработка типовых ошибок сборки и терминала ESP-IDF + LVGL

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

## Чек-лист перед push
- [ ] `idf.py build` проходит локально (или CI зелёный)
- [ ] новые .c добавлены в SRCS, зависимости — в REQUIRES
- [ ] CONFIG-флаги, влияющие на ввод/консоль, внесены в sdkconfig.defaults
- [ ] комментарии на русском, сообщения лога — на русском
