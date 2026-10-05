# Список исходников компонента main.
#
# ВАЖНО (ESP-IDF 6.x): зонтичный компонент "driver" больше НЕ подтягивает
# заголовки периферии автоматически — драйверы разбиты на независимые
# компоненты: esp_driver_uart, esp_driver_gpio, esp_driver_spi и т.д.
# Если указать просто "driver", компиляция падает с ошибкой:
#   fatal error: driver/uart.h: No such file or directory
# Поэтому в REQUIRES перечислены конкретные компоненты-драйверы.
#
# ВАЖНО: esp_vfs_dev — НЕ компонент (в IDF <=5.3 это часть компонента vfs,
# в IDF 6.0 функция esp_vfs_dev_uart_use_driver перенесена в driver/uart_vfs.h
# компонента esp_driver_uart). Указывать её в REQUIRES нельзя — CMake упадёт
# с "unknown name".
#
# Совместимость со старыми IDF (5.x): если специализированных компонентов
# ещё нет, автоматически добавляем зонтичный "driver".
set(MAIN_REQUIRES esp_driver_uart esp_driver_gpio esp_driver_spi
                  esp_lcd esp_partition app_update vfs
                  atanisoft__esp_lcd_ili9488)

if(NOT EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    # ESP-IDF <= 5.x: драйверы живут в зонтичном компоненте "driver"
    list(APPEND MAIN_REQUIRES driver)
endif()

idf_component_register(SRCS "main.c" "spi_freq_menu.c" "uart_input.c"
                    INCLUDE_DIRS "."
                    REQUIRES ${MAIN_REQUIRES})
