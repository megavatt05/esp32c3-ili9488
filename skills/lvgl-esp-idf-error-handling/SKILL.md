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

## 5. `Failed to resolve component 'mdns' required by component 'main': unknown name` (ESP-IDF 6.x)
**Причина:** в ESP-IDF 6.x компонент mDNS **удалён из дерева IDF** — он больше не
встроен, а распространяется как отдельный managed-компонент `espressif/mdns`.
Просто написать `mdns` в REQUIRES недостаточно: его нужно сначала добавить как
зависимость проекта. Также имя компонента отличается между версиями IDF:
в 5.x — `mdns`, в 6.x после установки — `espressif__mdns`.
**Лечение (оба шага обязательны):**
1. `main/idf_component.yml`:
   ```yaml
   dependencies:
     espressif/mdns: "^1.2.0"
   ```
2. `main/CMakeLists.txt` — разрешать имя динамически (проект может собираться
   и под IDF 5.x, и под 6.x). ВАЖНО: managed-компоненты лежат в
   `<корень проекта>/managed_components`, а НЕ рядом с `main/`; проверять нужно
   оба расположения плюс дерево IDF, иначе EXISTS всегда false и в REQUIRES
   попадает голое `mdns` → та же ошибка «unknown name»:
   ```cmake
   set(MDNS_RESOLVED FALSE)
   foreach(_root "${PROJECT_DIR}/managed_components"
                 "${CMAKE_CURRENT_LIST_DIR}/../managed_components"
                 "$ENV{IDF_PATH}/components")
       foreach(_mdn espressif__mdns mdns)
           if(EXISTS "${_root}/${_mdn}/CMakeLists.txt")
               list(APPEND MAIN_REQUIRES ${_mdn})
               set(MDNS_RESOLVED TRUE)
               break()
           endif()
       endforeach()
       if(MDNS_RESOLVED)
           break()
       endif()
   endforeach()
   if(NOT MDNS_RESOLVED)
       # First configure: managed_components ещё не скачан — dependency manager
       # зарегистрирует компонент сам, предполагаем имя IDF 6.x:
       list(APPEND MAIN_REQUIRES espressif__mdns)
   endif()
   ```
**Проверка:** `ls managed_components | grep mdns` после первого configure;
в коде include всегда `#include "mdns.h"` (префикс имени компонента в путь не входит).
Если ошибка осталась — `idf.py fullclean && idf.py reconfigure` (stale build-дир
кэширует старый список зависимостей).

## 6. Страница captive portal не открывается автоматически на телефоне
**Симптомы:** Wi-Fi подключается, но браузер не всплывает / «Нет интернета» без редиректа.
**Причины и лечение:**
- Android проверяет портал запросом `http://connectivitycheck.../generate_204` и
  игнорирует чистый `302`. Отдавать на пути проверок (`hotspot-detect`, `gen_204`,
  `hwdetect`, `detectportal`) HTML с `<meta http-equiv='refresh'>` вместо 302.
- `bind()` порта 53 падает без `SO_REUSEADDR`, если сокет уже занят — ставить опцию до bind.
- В DHCP-опции маршрутизатора должен отдаваться AP-шлюз как DNS (esp_netif DHCPServer
  делает это по умолчанию — не переопределять вручную).

## 7. Чек-лист перед push ветки с LVGL
- [ ] `.gitignore` содержит `build/` и `managed_components/`
- [ ] define LV_LVGL_H_INCLUDE_SIMPLE есть и в CMakeLists, и в sdkconfig.defaults
- [ ] шрифты в SRCS, нет #include *.c в ui.c
- [ ] REQUIRES включает все используемые компоненты (esp_partition для проверок flash)
- [ ] внешние компоненты (mdns и т.п.) объявлены в idf_component.yml И разрешены динамически в CMakeLists
- [ ] после изменения idf_component.yml выполнен `del sdkconfig` не нужен, но нужен fullclean при смене IDF
- [ ] локально пройдена `idf.py build` до пуша
