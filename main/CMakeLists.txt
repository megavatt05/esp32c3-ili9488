idf_component_register(SRCS "main.c" "ui.c"
                    INCLUDE_DIRS "." "../assets/fonts" "../assets/images"
                    REQUIRES driver esp_lcd atanisoft__esp_lcd_ili9488 lvgl__lvgl espressif__esp_lvgl_port)

# When you add generated fonts/images, list them here, for example:
# idf_component_register(SRCS "main.c" "ui.c"
#                        "../assets/fonts/font_inter_20.c"
#                        "../assets/images/bg_dark.c"
#                        ...)
