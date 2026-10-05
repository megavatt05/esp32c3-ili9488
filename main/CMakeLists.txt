idf_component_register(SRCS "main.c" "ui.c"
                    # Uncomment these lines after copying the real .c font files
                    # into assets/fonts/
                    # "../assets/fonts/font_inter_16.c"
                    # "../assets/fonts/font_roboto_20.c"
                    # "../assets/fonts/font_montserrat_24.c"
                    # "../assets/fonts/font_notosans_28.c"
                    INCLUDE_DIRS "." "../assets/fonts" "../assets/images"
                    REQUIRES driver esp_lcd atanisoft__esp_lcd_ili9488 lvgl__lvgl espressif__esp_lvgl_port)
