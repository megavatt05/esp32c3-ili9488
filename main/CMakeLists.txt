# Список исходников компонента main (softap-webui).
#
# ВАЖНО (ESP-IDF 6.x): зонтичный компонент "driver" больше НЕ подтягивает
# заголовки периферии — драйверы разбиты на независимые компоненты
# (esp_driver_gpio, esp_driver_spi и т.д.). Указываем их явно.
set(MAIN_REQUIRES esp_driver_gpio esp_driver_spi
                  esp_lcd esp_partition esp_wifi esp_netif esp_event nvs_flash
                  esp_http_server lwip atanisoft__esp_lcd_ili9488
                  lvgl__lvgl espressif__esp_lvgl_port)

if(NOT EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    # ESP-IDF <= 5.x: драйверы живут в зонтичном компоненте "driver"
    list(APPEND MAIN_REQUIRES driver)
endif()

# --- Разрешение имени компонента mDNS -------------------------------------
# В ESP-IDF 6.x mDNS вынесен из IDF в managed-компонент espressif/mdns
# (см. idf_component.yml) и регистрируется под именем espressif__mdns.
set(MDNS_RESOLVED FALSE)
foreach(_root "${PROJECT_DIR}/managed_components"
              "${CMAKE_CURRENT_LIST_DIR}/../managed_components"
              "$ENV{IDF_PATH}/components")
    foreach(_mdn espressif__mdns mdns)
        if(EXISTS "${_root}/${_mdn}/CMakeLists.txt")
            list(APPEND MAIN_REQUIRES ${_mdn})
            set(MDNS_RESOLVED TRUE)
            message(STATUS "mDNS: использован компонент '${_mdn}' из ${_root}")
            break()
        endif()
    endforeach()
    if(MDNS_RESOLVED)
        break()
    endif()
endforeach()
if(NOT MDNS_RESOLVED)
    # Fallback: на IDF 6.x managed-компонент всё равно регистрируется под именем
    # espressif__mdns — добавляем его, чтобы idf_component_register не упал на
    # «unknown name», а ошибка стала понятной на этапе сборки.
    list(APPEND MAIN_REQUIRES espressif__mdns)
    message(WARNING "mDNS: каталог managed_components ещё не создан — предполагаем espressif__mdns (будет скачан dependency-manager)")
endif()

idf_component_register(SRCS "main.c" "ui.c" "softap.c"
                    "../assets/fonts/font_inter_16.c"
                    "../assets/fonts/font_roboto_20.c"
                    "../assets/fonts/font_montserrat_24.c"
                    "../assets/fonts/font_notosans_28.c"
                    INCLUDE_DIRS "." "../assets/fonts" "../assets/images"
                    REQUIRES ${MAIN_REQUIRES})

# Единый короткий путь подключения LVGL для всех файлов компонента (main.c, ui.c, шрифты)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE=1)
