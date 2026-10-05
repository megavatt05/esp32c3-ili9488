/*
 * Beautiful LVGL 9 UI with Cyrillic support
 * Rounded cards, smooth fonts, gradient-like background
 */

#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "UI";

// Simple gradient-like background using colored rectangles
static void create_background(lv_obj_t *parent)
{
    // Dark elegant background
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0f172a), 0);  // slate-900
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    // Decorative top accent bar
    lv_obj_t *accent = lv_obj_create(parent);
    lv_obj_set_size(accent, LV_PCT(100), 8);
    lv_obj_align(accent, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x3b82f6), 0); // blue-500
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_radius(accent, 0, 0);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_card(lv_obj_t *parent, int y_offset)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 280, 110);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y_offset);

    // Rounded beautiful card
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e293b), 0); // slate-800
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    return card;
}

void ui_init(void)
{
    ESP_LOGI(TAG, "Creating beautiful UI with Cyrillic...");

    lv_obj_t *scr = lv_screen_active();
    create_background(scr);

    // ===== Title =====
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Привет, мир!");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "ESP32-C3 + ILI9488 + LVGL 9");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 65);

    // ===== Card 1 - Status =====
    lv_obj_t *card1 = create_card(scr, 110);

    lv_obj_t *lbl1 = lv_label_create(card1);
    lv_label_set_text(lbl1, "Статус системы");
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl1, lv_color_hex(0x60a5fa), 0);
    lv_obj_align(lbl1, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val1 = lv_label_create(card1);
    lv_label_set_text(val1, "Всё работает отлично");
    lv_obj_set_style_text_font(val1, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(val1, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val1, LV_ALIGN_TOP_LEFT, 0, 32);

    // ===== Card 2 - Temperature style =====
    lv_obj_t *card2 = create_card(scr, 240);

    lv_obj_t *lbl2 = lv_label_create(card2);
    lv_label_set_text(lbl2, "Температура");
    lv_obj_set_style_text_font(lbl2, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl2, lv_color_hex(0x34d399), 0);
    lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val2 = lv_label_create(card2);
    lv_label_set_text(val2, "24.5 °C");
    lv_obj_set_style_text_font(val2, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(val2, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(val2, LV_ALIGN_TOP_LEFT, 0, 30);

    // ===== Card 3 - Info =====
    lv_obj_t *card3 = create_card(scr, 370);

    lv_obj_t *lbl3 = lv_label_create(card3);
    lv_label_set_text(lbl3, "Информация");
    lv_obj_set_style_text_font(lbl3, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl3, lv_color_hex(0xfbbf24), 0);
    lv_obj_align(lbl3, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *val3 = lv_label_create(card3);
    lv_label_set_text(val3, "Красивые скруглённые\nшрифты без тормозов");
    lv_obj_set_style_text_font(val3, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(val3, lv_color_hex(0xe2e8f0), 0);
    lv_obj_align(val3, LV_ALIGN_TOP_LEFT, 0, 30);

    // ===== Footer =====
    lv_obj_t *footer = lv_label_create(scr);
    lv_label_set_text(footer, "Сделано с любовью на ESP32-C3");
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x64748b), 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -20);

    ESP_LOGI(TAG, "UI created successfully");
}
