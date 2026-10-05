# Навык: отработка ошибок сборки ESP-IDF / LVGL

Чек-лист типовых ошибок и их исправлений для проектов ESP32-C3 + ILI9488 + LVGL 9.

## 1. Несуществующие константы esp_err_t (ошибка компиляции)

**Симптом:**
```
error: 'ESP_ERR_NOT_FIT' undeclared; did you mean 'ESP_ERR_NOT_FOUND'?
```

**Причина:** в коде использована выдуманная константа — в ESP-IDF нет `ESP_ERR_NOT_FIT`.

**Исправление:** использовать только реальные коды ошибок из `esp_err.h`:
- `ESP_ERR_INVALID_SIZE` — размер не помещается / не подходит (правильная замена для «не влезло»);
- `ESP_ERR_NOT_FOUND` — раздел/устройство/символ не найдены;
- `ESP_ERR_NO_MEM` — нет памяти;
- `ESP_FAIL` — общий сбой без детализации.

**Профилактика:** перед коммитом grep по нестандартным символам:
`grep -rn "ESP_ERR_" main/ | sort -u` и сверка с esp_err.h.

## 2. fatal error: lvgl/lvgl.h: No such file or directory

**Причина:** сгенерированные lv_font_conv шрифты по умолчанию пишут `#include "lvgl/lvgl.h"`,
а в managed_components путь другой.

**Исправление (три слоя):**
1. `target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)` в `main/CMakeLists.txt`;
2. `CONFIG_LV_LVGL_H_INCLUDE_SIMPLE=y` в `sdkconfig.defaults`;
3. guarded include с резервным путём в ui.c.

## 3. undefined reference to `font_*` при линковке .elf

**Причина:** файлы шрифтов не добавлены в SRCS и/или одновременно #include'ются в ui.c (дубли).

**Исправление:** компилировать шрифты отдельными объектами — добавить все `assets/fonts/*.c`
в `SRCS` в `main/CMakeLists.txt`, а в ui.c оставить только `LV_FONT_DECLARE(...)`.

## 4. Переполнение flash / IRAM

- **Симптом:** `region `iram0_0_seg' overflowed` — атрибуты IRAM_SRAM_ADDR конфликтуют; убрать лишние `IRAM_ATTR` или перевести функцию в PSRAM/flash.
- **Симптом:** большой `.rodata` из-за шрифтов (~1 МБ на кириллический набор):
  сузить диапазоны глифов в `lv_font_conv --range`, включить `--compress`,
  либо поднять `CONFIG_ESPTOOLPY_FLASHSIZE`.
- **Контроль:** функция `check_flash_space()` печатает % заполнения app-раздела;
  предупреждение при <5% свободного места.

## Чек-лист перед push

1. `idf.py build` проходит локально (bootloader + приложение линкуются);
2. grep нестандартных идентификаторов (`ESP_ERR_NOT_FIT` и т.п.);
3. шрифты в SRCS, в ui.c только DECLARE;
4. `LV_LVGL_H_INCLUDE_SIMPLE` определён;
5. вывод размера прошивки проверен на запас flash.
