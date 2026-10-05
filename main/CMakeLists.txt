# Список исходников компонента main.
# Сгенерированные LVGL-шрифты из assets/fonts/ компилируются как отдельные
# объекты и добавляются в SRCS — больше НЕ включаются через #include в ui.c
# (это вызывало ошибки линковки / дублирование символов).
#
# ВАЖНО: опция LV_LVGL_H_INCLUDE_SIMPLE включает в шрифтах и ui.c короткий
# путь #include "lvgl.h". Без неё сгенерированные lv_font_conv файлы ищут
# заголовок как "lvgl/lvgl.h" и сборка падает с ошибкой:
#   fatal error: lvgl/lvgl.h: No such file or directory
# Поэтому define добавляется явно — не полагаясь на sdkconfig.
idf_component_register(SRCS "main.c" "ui.c" "softap.c"
                    "../assets/fonts/font_inter_16.c"
                    "../assets/fonts/font_roboto_20.c"
                    "../assets/fonts/font_montserrat_24.c"
                    "../assets/fonts/font_notosans_28.c"
                    INCLUDE_DIRS "." "../assets/fonts" "../assets/images"
                    REQUIRES driver esp_lcd esp_partition esp_wifi esp_netif esp_event nvs_flash
                    esp_http_server mdns lwip atanisoft__esp_lcd_ili9488 lvgl__lvgl espressif__esp_lvgl_port)

# Единый короткий путь подключения LVGL для всех файлов компонента (main.c, ui.c, шрифты)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
