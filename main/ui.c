/*
 * Beautiful LVGL 9 UI with custom Cyrillic fonts + background photo
 *
 * Background image:
 *   Convert assets/images/foto.jpg → C array named "foto"
 *   using https://lvgl.io/tools/imageconverter
 *   Color format: RGB565, Output: C array
 *   Then place foto.c into assets/images/ and enable it in CMakeLists.txt
 */

#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "UI";

// Custom Cyrillic fonts (put the .c files into assets/fonts/)
LV_FONT_DECLARE(font_inter_16);
LV_FONT_DECLARE(font_roboto_20);
LV_FONT_DECLARE(font_montserrat_24);
LV_FONT_DECLARE(font_notosans_28);

// Background photo (converted from foto.jpg)
// Uncomment after you convert and add foto.c
// LV_IMAGE_DECLARE(foto);

static void create_background(lv_obj_t *parent)
{
    // Try to use photo if available, otherwise solid color
#if 0   // Change to 1 after adding foto.c
    lv_obj_t *bg = lv_image_create(parent);
    lv_image_set_src(bg, &foto);
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_align(bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_background(bg);
#else
    // Fallback dark background
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
#endif

    // Top accent bar
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
    lv_obj_set_style_bg_opa(card, LV_OPA_90, 0);          // slightly transparent over photo
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
    ESP_LOGI(TAG, "Creating UI...");

    lv_obj_t *scr = lv_screen_active();
    create_background(scr);

    // Title
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Привет, мир!");
    lv_obj_set_style_text_font(title, &font_notosans_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "ESP32-C3 + ILI9488 + LVGL 9");
    lv_obj_set_style_text_font(subtitle, &font_inter_16, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 58);

    // Card 1
    lv_obj_t *card1 = create_card(scr, 95, 100);
    lv_obj_t *lbl1 = lv_label_create(card1);
    lv_label_set_text(lbl1, "Статус системы");
    lv_obj_set_style_text_font(lbl1, &font_inter_16, 0);
    lv_obj_set_style_text_color(lbl1, lv_color_hex(0x60a5fa), 0);
    lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val1 = lv_label_create(card1);
    lv_label_set_text(val1, "Всё работает отлично");
    lv_obj_set_style_text_font(val1, &font_roboto_20, 0);
    lv_obj_set_style_text_color(val1, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val1, LV_ALIGN_TOP_LEFT, 0, 30);

    // Card 2
    lv_obj_t *card2 = create_card(scr, 210, 105);
    lv_obj_t *lbl2 = lv_label_create(card2);
    lv_label_set_text(lbl2, "Температура");
    lv_obj_set_style_text_font(lbl2, &font_inter_16, 0);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0x34d399), 0);
    lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val2 = lv_label_create(card2);
    lv_label_set_text(val2, "24.5 °C");
    lv_obj_set_style_text_font(val2, &font_montserrat_24, 0);
    lv_obj_set_style_text_color(val2, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val2, LV_ALIGN_TOP_LEFT, 0, 28);

    // Card 3
    lv_obj_t *card3 = create_card(scr, 330, 120);
    lv_obj_t *lbl3 = lv_label_create(card3);
    lv_label_set_text(lbl3, "Демонстрация шрифтов");
    lv_obj_set_style_text_font(lbl3, &font_inter_16, 0);
    lv_obj_set_style_text_color(lbl3, lv_color_hex(0xfbbf24), 0);
    lv_obj_align(lbl3, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val3 = lv_label_create(card3);
    lv_label_set_text(val3, "Inter 16 • Roboto 20\nMontserrat 24 • Noto 28");
    lv_obj_set_style_text_font(val3, &font_roboto_20, 0);
    lv_obj_set_style_text_color(val3, lv_color_hex(0xe2e8f0), 0);
    lv_obj_align(val3, LV_ALIGN_TOP_LEFT, 0, 30);

    // Footer
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Сделано с любовью на ESP32-C3");
    lv_obj_set_style_text_font(footer, &font_inter_16, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x64748b), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -16);

    ESP_LOGI(TAG, "UI created");
}
