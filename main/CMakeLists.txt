idf_component_register(SRCS "main.c" "spi_freq_menu.c" "uart_input.c"
                    INCLUDE_DIRS "."
                    # ВАЖНО: esp_vfs_dev — НЕ компонент (в IDF <=5.3 это часть компонента vfs,
                    # в IDF 6.0 функция esp_vfs_dev_uart_use_driver перенесена в драйвер uart).
                    # Указывать её в REQUIRES нельзя — CMake упадёт с "unknown name".
                    REQUIRES driver esp_lcd esp_partition app_update vfs atanisoft__esp_lcd_ili9488)
