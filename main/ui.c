/*
 * Красивый UI на LVGL 9 с кастомными кириллическими шрифтами и фото-фоном
 *
 * Фоновое изображение:
 *   Конвертируйте assets/images/foto.jpg → C-массив с именем "foto"
 *   через https://lvgl.io/tools/imageconverter
 *   Формат цвета: RGB565, вывод: C-массив
 *   Затем положите foto.c в assets/images/ и включите его в CMakeLists.txt
 */

#include <stdio.h>
// ВАЖНО: системные заголовки должны идти ДО "ui.h" — ui.h включает lvgl.h,
// который в ESP-IDF 6.x тянет сетевые заголовки (lwip/sockets.h), задающие
// макросы close/read/write и конфликтующие со стандартными <unistd.h>.
// Порядок гарантирует корректную видимость BSD-сокет API во всех .c файлах.
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ui.h"
// Подключение LVGL: короткий путь "lvgl.h".
// Макрос LV_LVGL_H_INCLUDE_SIMPLE определён в main/CMakeLists.txt через
// target_compile_definitions — тот же путь используется в сгенерированных
// шрифтах (assets/fonts/*.c), иначе сборка падает с ошибкой
// "fatal error: lvgl/lvgl.h: No such file or directory".
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h" // резервный путь, если define не передан
#endif
#include "esp_log.h"
#include "esp_lvgl_port.h"

// ---------------------------------------------------------------------------
// Кастомные шрифты с поддержкой кириллицы
//
// Сгенерированные LVGL-шрифты находятся в assets/fonts/ и компилируются как
// отдельные файлы сборки (перечислены в SRCS в main/CMakeLists.txt).
// Здесь они НЕ включаются через #include — иначе получили бы дублирование
// определений и ошибки линковки (undefined reference / multiple definition).
// Ниже объявлены только внешние символы lv_font_t.
// ---------------------------------------------------------------------------
LV_FONT_DECLARE(font_inter_16);      // Основной текст, 16 px
LV_FONT_DECLARE(font_roboto_20);     // Значения, 20 px
LV_FONT_DECLARE(font_montserrat_24); // Крупный текст, 24 px
LV_FONT_DECLARE(font_notosans_28);   // Заголовок, 28 px

static const char *TAG = "UI";

// Объекты экрана, используемые веб-управлением (объявлены в начале файла,
// чтобы быть видимыми и в ui_init(), и в функциях SoftAP в конце файла)
static lv_obj_t *s_main_screen = NULL;    // основной экран демо (для возврата с веб-баннера)
static lv_obj_t *s_ap_ssid_label = NULL;  // метка SSID на баннере точки доступа

#define FONT_BODY  &font_inter_16      // Шрифт основного текста
#define FONT_VALUE &font_roboto_20     // Шрифт значений
#define FONT_BIG   &font_montserrat_24 // Крупный шрифт
#define FONT_TITLE &font_notosans_28   // Шрифт заголовка

// Фоновое фото (сконвертировано из foto.jpg)
// Раскомментируйте после конвертации и добавления foto.c
// LV_IMAGE_DECLARE(foto);

static void create_background(lv_obj_t *parent)
{
    // Пытаемся использовать фото, если доступно; иначе — однотонный фон
#if 0   // Замените на 1 после добавления foto.c
    lv_obj_t *bg = lv_image_create(parent);
    lv_image_set_src(bg, &foto);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(bg);
#else
    // Запасной тёмный фон
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
#endif

    // Акцентная полоса сверху
    lv_obj_t *accent = lv_obj_create(parent);
    lv_obj_set_size(accent, LV_PCT(100), 6);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_card(lv_obj_t *parent, int y_offset, int height)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 290, height);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y_offset);

    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);          // слегка прозрачная поверх фото
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_shadow_width(card, 16, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}

void ui_init(void)
{
    ESP_LOGI(TAG, "Создание UI...");

    lv_obj_t *scr = lv_screen_active();
    s_main_screen = scr;   // запоминаем основной экран для возврата с веб-демо
    create_background(scr);

    // Заголовок
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Привет, мир!");
    lv_obj_set_style_text_font(title, FONT_TITLE, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "ESP32-C3 + ILI9488 + LVGL 9");
    lv_obj_set_style_text_font(subtitle, FONT_BODY, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 58);

    // Карточка 1
    lv_obj_t *card1 = create_card(scr, 95, 100);
    lv_obj_t *lbl1 = lv_label_create(card1);
    lv_label_set_text(lbl1, "Статус системы");
    lv_obj_set_style_text_font(lbl1, FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl1, lv_color_hex(0x60a5fa), 0);
    lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val1 = lv_label_create(card1);
    lv_label_set_text(val1, "Всё работает отлично");
    lv_obj_set_style_text_font(val1, FONT_VALUE, 0);
    lv_obj_set_style_text_color(val1, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val1, LV_ALIGN_TOP_LEFT, 0, 30);

    // Карточка 2
    lv_obj_t *card2 = create_card(scr, 210, 105);
    lv_obj_t *lbl2 = lv_label_create(card2);
    lv_label_set_text(lbl2, "Температура");
    lv_obj_set_style_text_font(lbl2, FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0x34d399), 0);
    lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val2 = lv_label_create(card2);
    lv_label_set_text(val2, "24.5 °C");
    lv_obj_set_style_text_font(val2, FONT_BIG, 0);
    lv_obj_set_style_text_color(val2, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val2, LV_ALIGN_TOP_LEFT, 0, 28);

    // Карточка 3
    lv_obj_t *card3 = create_card(scr, 330, 120);
    lv_obj_t *lbl3 = lv_label_create(card3);
    lv_label_set_text(lbl3, "Демонстрация шрифтов");
    lv_obj_set_style_text_font(lbl3, FONT_BODY, 0);
    lv_obj_set_style_text_color(lbl3, lv_color_hex(0xfbbf24), 0);
    lv_obj_align(lbl3, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val3 = lv_label_create(card3);
    lv_label_set_text(val3, "Inter 16 • Roboto 20\nMontserrat 24 • Noto 28");
    lv_obj_set_style_text_font(val3, FONT_VALUE, 0);
    lv_obj_set_style_text_color(val3, lv_color_hex(0xe2e8f0), 0);
    lv_obj_align(val3, LV_ALIGN_TOP_LEFT, 0, 30);

    // Нижняя подпись
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Сделано с любовью на ESP32-C3");
    lv_obj_set_style_text_font(footer, FONT_BODY, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x64748b), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -16);

    ESP_LOGI(TAG, "UI создан");
}

/* ================= Веб-управление (SoftAP) ================= */

// Ссылки на объекты, доступные из обработчиков HTTP-запросов
static lv_obj_t *s_web_screen = NULL;    // отдельный экран веб-демо
static bool s_web_screen_created = false;

/**
 * @brief Установить цвет фона. Если пользователь уже открыл веб-экран —
 * перекрашивается он, иначе фон основного экрана.
 * Вызывается из задачи HTTP-сервера, поэтому блокирует LVGL через esp_lvgl_port.
 */
void ui_set_bg_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!lvgl_port_lock(100)) {
        ESP_LOGW(TAG, "Не удалось взять блокировку LVGL для смены цвета");
        return;
    }

    lv_color_t col = lv_color_hex(((uint32_t)r << 16) | ((uint32_t)g << 8) | b);

    if (s_web_screen_created) {
        // Перекрашиваем веб-экран
        lv_obj_set_style_bg_color(s_web_screen, col, 0);
    } else {
        // Меняем фон активного (основного) экрана
        lv_obj_set_style_bg_color(lv_screen_active(), col, 0);
        lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    }

    lvgl_port_unlock();
}

/**
 * @brief Показать/скрыть баннер точки доступа на экране.
 * Вызывается из обработчика событий Wi-Fi при подключении клиента.
 */
void ui_show_ap_banner(bool show, const char *ssid)
{
    if (!lvgl_port_lock(100)) {
        ESP_LOGW(TAG, "Блокировка LVGL недоступна (баннер AP)");
        return;
    }

    if (show) {
        if (!s_web_screen_created) {
            s_web_screen = lv_obj_create(NULL);   // новый экран (screen)
            lv_obj_remove_style_all(s_web_screen);
            lv_obj_set_style_bg_color(s_web_screen, lv_color_hex(0x111827), 0);
            lv_obj_set_style_bg_opa(s_web_screen, LV_OPA_COVER, 0);
            s_web_screen_created = true;

            lv_obj_t *t = lv_label_create(s_web_screen);
            lv_label_set_text(t, "SOFTAP ONLINE");
            lv_obj_set_style_text_font(t, FONT_TITLE, 0);
            lv_obj_set_style_text_color(t, lv_color_hex(0x4fc3f7), 0);
            lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 40);

            s_ap_ssid_label = lv_label_create(s_web_screen);
            lv_obj_set_style_text_font(s_ap_ssid_label, FONT_VALUE, 0);
            lv_obj_set_style_text_color(s_ap_ssid_label, lv_color_hex(0xe5e7eb), 0);
            lv_obj_align(s_ap_ssid_label, LV_ALIGN_BOTTOM_MID, 0, -40);
        }
        if (ssid && s_ap_ssid_label) {
            char buf[64];
            snprintf(buf, sizeof(buf), "SSID: %s\nhttp://192.168.4.1", ssid);
            lv_label_set_text(s_ap_ssid_label, buf);
        }
        lv_screen_load(s_web_screen);
    } else if (s_web_screen_created && s_main_screen) {
        // Возврат на основной экран демо
        lv_screen_load(s_main_screen);
    }

    lvgl_port_unlock();
}
