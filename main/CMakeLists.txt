# Список исходников компонента main.
# Сгенерированные LVGL-шрифты из assets/fonts/ компилируются как отдельные
# объекты и добавляются в SRCS — больше НЕ включаются через #include в ui.c
# (это вызывало ошибки линковки / дублирование символов).
#
# ВАЖНО (ESP-IDF 6.x): зонтичный компонент "driver" больше НЕ подтягивает
# заголовки периферии — драйверы разбиты на независимые компоненты
# (esp_driver_gpio, esp_driver_spi). Указываем их явно.
set(MAIN_REQUIRES esp_driver_gpio esp_driver_spi
                  esp_lcd esp_partition atanisoft__esp_lcd_ili9488
                  lvgl__lvgl espressif__esp_lvgl_port)

if(NOT EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    # ESP-IDF <= 5.x: драйверы живут в зонтичном компоненте "driver"
    list(APPEND MAIN_REQUIRES driver)
endif()

idf_component_register(SRCS "main.c" "ui.c"
                    "../assets/fonts/font_inter_16.c"
                    "../assets/fonts/font_roboto_20.c"
                    "../assets/fonts/font_montserrat_24.c"
                    "../assets/fonts/font_notosans_28.c"
                    INCLUDE_DIRS "." "../assets/fonts" "../assets/images"
                    REQUIRES ${MAIN_REQUIRES})

# ВАЖНО: опция LV_LVGL_H_INCLUDE_SIMPLE включает в шрифтах и ui.c короткий
# путь #include "lvgl.h". Без неё сгенерированные lv_font_conv файлы ищут
# заголовок как "lvgl/lvgl.h" и сборка падает с ошибкой:
#   fatal error: lvgl/lvgl.h: No such file or directory
# Поэтому define добавляется явно — не полагаясь на sdkconfig.
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
