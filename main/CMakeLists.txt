# В ESP-IDF 6.x драйвер UART вынесен в отдельный компонент esp_driver_uart,
# поэтому в REQUIRES указываем именно его (а не зонтичный "driver").
idf_component_register(SRCS         "uart_echo_example_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES     esp_driver_uart)
