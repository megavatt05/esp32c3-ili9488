# Навык: отработка типовых ошибок сборки LVGL 9 + ESP-IDF

## 1. `fatal error: lvgl/lvgl.h: No such file or directory` (в шрифтах assets/fonts/*.c)
**Причина:** сгенерированные `lv_font_conv` файлы по умолчанию делают
`#include "lvgl/lvgl.h"`. В managed-компоненте `lvgl__lvgl` заголовок лежит в
корне `src/`, путь `lvgl/...` не резолвится.
**Лечение (все три слоя сразу):**
1. `main/CMakeLists.txt`: `target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)` — гарантирует define независимо от sdkconfig;
2. `sdkconfig.defaults`: `CONFIG_LV_LVGL_H_INCLUDE_SIMPLE=y`;
3. В своих файлах писать guarded include:
   ```c
   #ifdef LV_LVGL_H_INCLUDE_SIMPLE
   #include "lvgl.h"
   #else
   #include "lvgl/lvgl.h"
   #endif
   ```
**Проверка:** `grep -rn "LV_LVGL_H_INCLUDE_SIMPLE" main/ sdkconfig.defaults` — должно быть определено и там, и там.

## 2. `undefined reference to font_xxx` при линковке .elf
**Причина:** шрифты либо исключены из SRCS, либо одновременно #include в ui.c и в SRCS (duplicate definition).
**Лечение:** компилировать шрифты отдельными объектами через SRCS (`"../assets/fonts/font_*.c"`), в ui.c только `LV_FONT_DECLARE(...)`. Никаких `#include` .c-файлов шрифтов.

## 3. `ignoring attribute 'section (".iram1.xx")' because it conflicts`
**Причина:** `LV_ATTRIBUTE_FAST_MEM_USE_IRAM` конфликтует с GCC 12+/IDF 5.x+.
**Лечение:** `CONFIG_LV_ATTRIBUTE_FAST_MEM_USE_IRAM=n` в sdkconfig.defaults.

## 4. Переполнение flash из-за шрифтов (~1 МБ на 4 кириллических шрифта)
**Профилактика:** генерировать минимальные диапазоны глифов, `--compress`,
проверять размер до прошивки: `idf.py size` / скрипт проверки свободных байт
раздела app. Предупреждение при заполнении >95%.

## 5. Чек-лист перед push ветки с LVGL
- [ ] `.gitignore` содержит `build/` и `managed_components/`
- [ ] define LV_LVGL_H_INCLUDE_SIMPLE есть и в CMakeLists, и в sdkconfig.defaults
- [ ] шрифты в SRCS, нет #include *.c в ui.c
- [ ] REQUIRES включает все используемые компоненты (esp_partition для проверок flash)
- [ ] локально пройдена `idf.py build` до пуша
