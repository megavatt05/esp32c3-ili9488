# UART-only. Без дисплея и Wi-Fi.
#
# Правило ESP-IDF: каждый #include "xxx.h" требует явного
# REQUIRES на компонент, который даёт этот заголовок.
#   driver/uart.h      → esp_driver_uart  (IDF 5.3+/6.x)
#   driver/uart_vfs.h  → esp_driver_uart
#   esp_timer.h        → esp_timer
#   esp_system.h       → esp_hw_support / esp_system (часто тянется через другие)
#
# Документация:
#   API Reference → Storage → VFS
#   Migration Guides → 5.3 → Storage
#   Build System → Component Requirements

set(MAIN_REQUIRES vfs esp_timer)

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
