idf_component_register(SRCS "main.c" "ui.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd atanisoft__esp_lcd_ili9488 lvgl__lvgl espressif__esp_lvgl_port)
