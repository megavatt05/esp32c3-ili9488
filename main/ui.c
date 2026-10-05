/*
 * Красивый UI на LVGL 9 с кастомными кириллическими шрифтами и фото-фоном
 *
 * Фоновое изображение:
 *   Конвертируйте assets/images/foto.jpg → C-массив с именем "foto"
 *   через https://lvgl.io/tools/imageconverter
 *   Формат цвета: RGB565, вывод: C-массив
 *   Затем положите foto.c в assets/images/ и включите его в CMakeLists.txt
 */

#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"

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
