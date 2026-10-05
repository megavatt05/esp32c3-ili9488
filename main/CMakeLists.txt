idf_component_register(SRCS "main.c" "spi_freq_menu.c" "uart_input.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd esp_partition app_update esp_vfs_dev atanisoft__esp_lcd_ili9488)
