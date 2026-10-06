/*
 * ESP-IDF 6.0.3: ILI9488 SPI на ESP32-C3 Super Mini
 *
 * Частота SPI задаётся из терминала:
 *   при старте — меню 45 с;
 *   во время демо — в любой момент: 1..8 или 40M + Enter.
 *
 * Pinout: CS=GPIO5 RST=GPIO0 DC=GPIO1 MOSI=GPIO4 SCK=GPIO2
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9488.h"
#include "spi_freq_menu.h"
#include "uart_input.h"

static const char *TAG = "ILI9488";

#define LCD_HOST            SPI2_HOST
#define PIN_NUM_SCLK        2
#define PIN_NUM_MOSI        4
#define PIN_NUM_MISO        -1
#define PIN_NUM_LCD_CS      5
#define PIN_NUM_LCD_DC      1
#define PIN_NUM_LCD_RST     0

#define LCD_H_RES           320
#define LCD_V_RES           480
#define LCD_BUFFER_LINES    40
#define LCD_BUFFER_SIZE     (LCD_H_RES * LCD_BUFFER_LINES)

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static int s_freq_mhz = 40;
static bool s_spi_bus_ready = false;

static esp_err_t check_flash_space(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Не удалось получить информацию о разделе приложения");
        return ESP_FAIL;
    }

    size_t part_size = running->size;
    uint8_t buf[256];
    size_t used = part_size;
    for (ssize_t off = (ssize_t)part_size - (ssize_t)sizeof(buf); off >= 0; off -= sizeof(buf)) {
        if (esp_partition_read(running, off, buf, sizeof(buf)) != ESP_OK) {
            break;
        }
        bool all_ff = true;
        for (size_t i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF) {
                all_ff = false;
                break;
            }
        }
        if (!all_ff) {
            used = off + sizeof(buf);
            break;
        }
        used = off;
    }

    size_t free_bytes = part_size - used;
    int percent_used = (int)((used * 100) / part_size);
    ESP_LOGI(TAG, "Flash-раздел '%s': размер %u КБ, занято ~%u КБ (%d%%), свободно ~%u КБ",
             running->label, (unsigned)(part_size / 1024),
             (unsigned)(used / 1024), percent_used,
             (unsigned)(free_bytes / 1024));

    if (free_bytes < part_size / 20) {
        ESP_LOGW(TAG, "ВНИМАНИЕ: в flash-разделе осталось менее 5%% места!");
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static void fill_color(uint16_t color)
{
    if (!panel_handle) {
        return;
    }
    uint16_t *buf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        return;
    }
    for (int i = 0; i < LCD_H_RES; i++) {
        buf[i] = color;
    }
    for (int y = 0; y < LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 1, buf);
    }
    free(buf);
}

typedef struct {
    uint16_t color;
    const char *name;
} color_item_t;

static const color_item_t COLORS[] = {
    { 0xF800, "RED" },
    { 0x07E0, "GREEN" },
    { 0x001F, "BLUE" },
    { 0xFFFF, "WHITE" },
    { 0x0000, "BLACK" },
    { 0xFFE0, "YELLOW" },
    { 0xF81F, "MAGENTA" },
    { 0x07FF, "CYAN" },
};
#define NUM_COLORS (sizeof(COLORS) / sizeof(COLORS[0]))

static void draw_text(const char *text, uint16_t fg, uint16_t bg)
{
    if (!panel_handle) {
        return;
    }
    static const char *GLYPHS[36][7] = {
        {"01110","10001","10001","11111","10001","10001","10001"},
        {"11110","10001","10001","11110","10001","10001","11110"},
        {"01111","10000","10000","10000","10000","10000","01111"},
        {"11110","10001","10001","10001","10001","10001","11110"},
        {"11111","10000","10000","11110","10000","10000","11111"},
        {"11111","10000","10000","11110","10000","10000","10000"},
        {"01111","10000","10000","10111","10001","10001","01111"},
        {"10001","10001","10001","11111","10001","10001","10001"},
        {"11111","00100","00100","00100","00100","00100","11111"},
        {"11111","01000","01000","01000","01001","01001","11110"},
        {"10001","10010","10100","11000","10100","10010","10001"},
        {"10000","10000","10000","10000","10000","10000","11111"},
        {"10001","11011","10101","10101","10001","10001","10001"},
        {"10001","11001","10101","10011","10001","10001","10001"},
        {"01110","10001","10001","10001","10001","10001","01110"},
        {"11110","10001","10001","11110","10000","10000","10000"},
        {"01110","10001","10001","10001","10101","10010","01101"},
        {"11110","10001","10001","11110","10100","10010","10001"},
        {"01111","10000","10000","01110","00001","00001","11110"},
        {"11111","00100","00100","00100","00100","00100","00100"},
        {"10001","10001","10001","10001","10001","10001","01110"},
        {"10001","10001","10001","10001","10001","01010","00100"},
        {"10001","10001","10001","10101","10101","11011","10001"},
        {"10001","10001","01010","00100","01010","10001","10001"},
        {"10001","10001","01010","00100","00100","00100","00100"},
        {"11111","00001","00010","00100","01000","10000","11111"},
        {"01110","10001","10011","10101","11001","10001","01110"},
        {"00100","01100","00100","00100","00100","00100","01110"},
        {"01110","10001","00001","00010","00100","01000","11111"},
        {"11111","00010","00100","00010","00001","10001","01110"},
        {"00010","00110","01010","10010","11111","00010","00010"},
        {"11111","10000","11110","00001","00001","10001","01110"},
        {"01110","10000","10000","11110","10001","10001","01110"},
        {"11111","00001","00010","00100","01000","01000","01000"},
        {"01110","10001","10001","01110","10001","10001","01110"},
        {"01110","10001","10001","01111","00001","00001","01110"},
    };
    const int SCALE = 6;
    const int CHAR_W = 5 * SCALE;
    const int CHAR_H = 7 * SCALE;
    const int GAP = SCALE;

    int len = (int)strlen(text);
    int total_w = len * CHAR_W + (len > 0 ? (len - 1) : 0) * GAP;
    if (total_w > LCD_H_RES) {
        return;
    }
    int x0 = (LCD_H_RES - total_w) / 2;
    int y0 = (LCD_V_RES - CHAR_H) / 2;

    uint16_t *rowbuf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!rowbuf) {
        return;
    }

    for (int ry = 0; ry < CHAR_H; ry++) {
        for (int x = 0; x < LCD_H_RES; x++) {
            rowbuf[x] = bg;
        }
        for (int ci = 0; ci < len; ci++) {
            char ch = text[ci];
            if (ch == ' ') {
                continue;
            }
            int gi = -1;
            if (ch >= 'A' && ch <= 'Z') {
                gi = ch - 'A';
            } else if (ch >= 'a' && ch <= 'z') {
                gi = ch - 'a';
            } else if (ch >= '0' && ch <= '9') {
                gi = 26 + (ch - '0');
            }
            if (gi < 0) {
                continue;
            }
            int glyph_row = ry / SCALE;
            if (glyph_row > 6) {
                continue;
            }
            const char *bits = GLYPHS[gi][glyph_row];
            int cx = x0 + ci * (CHAR_W + GAP);
            for (int b = 0; b < 5; b++) {
                if (bits[b] == '1') {
                    int px = cx + b * SCALE;
                    for (int s = 0; s < SCALE; s++) {
                        if (px + s >= 0 && px + s < LCD_H_RES) {
                            rowbuf[px + s] = fg;
                        }
                    }
                }
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y0 + ry, LCD_H_RES, y0 + ry + 1, rowbuf);
    }
    free(rowbuf);
}

static esp_err_t lcd_start(int freq_mhz)
{
    if (panel_handle) {
        esp_lcd_panel_del(panel_handle);
        panel_handle = NULL;
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
        io_handle = NULL;
    }

    if (!s_spi_bus_ready) {
        spi_bus_config_t buscfg = {
            .sclk_io_num = PIN_NUM_SCLK,
            .mosi_io_num = PIN_NUM_MOSI,
            .miso_io_num = PIN_NUM_MISO,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = LCD_H_RES * LCD_BUFFER_LINES * sizeof(uint16_t) + 8,
        };
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
        s_spi_bus_ready = true;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = freq_mhz * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 18,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config, LCD_BUFFER_SIZE, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    s_freq_mhz = freq_mhz;
    ESP_LOGI(TAG, "SPI частота установлена: %d МГц", freq_mhz);
    return ESP_OK;
}

static void show_freq_on_lcd(int mhz)
{
    char freq_str[24];
    snprintf(freq_str, sizeof(freq_str), "%dMHZ", mhz);
    fill_color(0x0000);
    draw_text(freq_str, 0xFFFF, 0x0000);
}

static bool poll_freq_change(int wait_ms)
{
    char line[32];
    int n = uart_input_line(line, sizeof(line), wait_ms);
    if (n <= 0) {
        return false;
    }
    int mhz = spi_freq_parse_mhz(line);
    if (mhz <= 0) {
        printf("Некорректно '%s'. Пример: 2  или  40M\n> ", line);
        fflush(stdout);
        return false;
    }
    if (mhz == s_freq_mhz) {
        printf("Уже %d МГц\n> ", mhz);
        fflush(stdout);
        return false;
    }
    printf("Меняю SPI на %d МГц...\n", mhz);
    fflush(stdout);
    if (lcd_start(mhz) != ESP_OK) {
        ESP_LOGE(TAG, "Не удалось сменить частоту");
        return false;
    }
    show_freq_on_lcd(mhz);
    printf("SPI шина работает на частоте %d МГц\n> ", mhz);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(800));
    return true;
}

static void color_cycle_demo(void)
{
    printf("Демо цветов. Частоту можно сменить в любой момент:\n");
    printf("  1=10МГц  2=20  3=30  4=40  5=50  6=60  7=70  8=80\n");
    printf("  или 45M + Enter\n> ");
    fflush(stdout);

    while (1) {
        for (size_t c = 0; c < NUM_COLORS; c++) {
            uint16_t bg = COLORS[c].color;
            uint16_t fg = (bg == 0x0000 || bg == 0x001F || bg == 0xF81F || bg == 0x07E0)
                          ? 0xFFFF : 0x0000;
            fill_color(bg);
            draw_text(COLORS[c].name, fg, bg);
            ESP_LOGI(TAG, "Цвет %d/%u: %s @ %d МГц",
                     (int)c + 1, (unsigned)NUM_COLORS, COLORS[c].name, s_freq_mhz);
            (void)poll_freq_change(1500);
        }
    }
}

void app_main(void)
{
    uart_input_init();
    check_flash_space();

    int freq_mhz = spi_freq_select_mhz();
    printf("Итоговая частота шины SPI: %d МГц (%d Гц)\n", freq_mhz, freq_mhz * 1000000);

    ESP_LOGI(TAG, "Инициализация дисплея...");
    ESP_ERROR_CHECK(lcd_start(freq_mhz));

    printf("SPI шина работает на частоте %d МГц\n", freq_mhz);
    show_freq_on_lcd(freq_mhz);
    vTaskDelay(pdMS_TO_TICKS(1200));

    ESP_LOGI(TAG, "Дисплей готов");
    color_cycle_demo();
}
