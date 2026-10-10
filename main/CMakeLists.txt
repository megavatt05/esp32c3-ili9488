# CMakeLists.txt for main component
#
# ESP-IDF 5.x / 6.x compatibility:
# - In IDF 6.x the old "driver" component was split into
#   esp_driver_uart, esp_driver_gpio, esp_driver_spi, ...
# - In IDF 5.x everything still lives under the umbrella "driver" component.
#
# We therefore always list the new components and, if they do not exist,
# fall back to the old "driver" component.

set(MAIN_REQUIRES
    esp_lcd
    esp_partition
    app_update
    vfs
    atanisoft__esp_lcd_ili9488
)

# UART / GPIO / SPI drivers
if(EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    # ESP-IDF 5.3+ / 6.x
    list(APPEND MAIN_REQUIRES
        esp_driver_uart
        esp_driver_gpio
        esp_driver_spi
    )
else()
    # Older ESP-IDF (5.0 - 5.2)
    list(APPEND MAIN_REQUIRES driver)
endif()

idf_component_register(
    SRCS
        "main.c"
        "spi_freq_menu.c"
        "uart_input.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        ${MAIN_REQUIRES}
    # nvs_flash нужен только uart_input.c (nvs_flash_init) - приватная зависимость
    PRIV_REQUIRES
        nvs_flash
)
