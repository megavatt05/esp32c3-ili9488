idf_component_register(SRCS "main.c" "spi_freq_menu.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd esp_partition app_update atanisoft__esp_lcd_ili9488)
