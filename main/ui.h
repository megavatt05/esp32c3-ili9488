#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Инициализация пользовательского интерфейса LVGL (вызывать под блокировкой esp_lvgl_port). */
void ui_init(void);

/** Установить цвет фона экрана из веб-интерфейса (функция сама берёт блокировку LVGL). */
void ui_set_bg_color(uint8_t r, uint8_t g, uint8_t b);

/** Показать/скрыть баннер точки доступа на экране. */
void ui_show_ap_banner(bool show, const char *ssid);

#ifdef __cplusplus
}
#endif
