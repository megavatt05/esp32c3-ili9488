# Только UART. Без дисплея, без Wi-Fi, без LCD-компонентов.
set(MAIN_REQUIRES vfs)

if(EXISTS "$ENV{IDF_PATH}/components/esp_driver_uart")
    list(APPEND MAIN_REQUIRES esp_driver_uart esp_driver_gpio)
else()
    list(APPEND MAIN_REQUIRES driver)
endif()

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES ${MAIN_REQUIRES}
)
