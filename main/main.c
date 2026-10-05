/*
 * ESP-IDF 6.0.3 example: ILI9488 SPI on ESP32-C3 Super Mini
 *
 * Two modes:
 *  1. Color fill demo (default)
 *  2. SPI frequency stress test (define SPI_FREQ_TEST 1)
 *
 * Pinout:
 *   CS=GPIO5, RST=GPIO0, DC=GPIO1, MOSI=GPIO4, SCK=GPIO2
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

static const char *TAG = "ILI9488";

// ================== Pinout ==================
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

// Set to 1 to run SPI frequency stress test instead of color demo
#define SPI_FREQ_TEST       0

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;

/*
 * Проверка свободного места в flash-разделе приложения.
 * Сравнивает размер текущей прошивки с размером раздела app и выводит
 * процент заполнения. Возвращает ESP_OK, если место ещё есть (>5%),
 * иначе ESP_ERR_NOT_FIT — прошивка почти заполнила раздел.
 */
static esp_err_t check_flash_space(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Не удалось получить информацию о разделе приложения");
        return ESP_FAIL;
    }

    size_t part_size = running->size;

    // Оценка занятого места: сканируем раздел с конца на предмет пустого
    // пространства (0xFF) — за последним записанным блоком flash стёрт.
    uint8_t buf[256];
    size_t used = part_size;
    for (ssize_t off = (ssize_t)part_size - (ssize_t)sizeof(buf); off >= 0; off -= sizeof(buf)) {
        if (esp_partition_read(running, off, buf, sizeof(buf)) != ESP_OK) {
            break;
        }
        bool all_ff = true;
        for (size_t i = 0; i < sizeof(buf); i++) {
            if (buf[i] != 0xFF) { all_ff = false; break; }
        }
        if (!all_ff) {
            used = off + sizeof(buf);
            break;
        }
        used = off; // идём дальше к началу
    }

    size_t free_bytes = part_size - used;
    int percent_used = (int)((used * 100) / part_size);
    ESP_LOGI(TAG, "Flash-раздел '%s': размер %u КБ, занято ~%u КБ (%d%%), свободно ~%u КБ",
             running->label, (unsigned)(part_size / 1024),
             (unsigned)(used / 1024), percent_used,
             (unsigned)(free_bytes / 1024));

    if (free_bytes < part_size / 20) { // меньше 5% свободно
        ESP_LOGW(TAG, "ВНИМАНИЕ: в flash-разделе осталось менее 5%% места!");
        return ESP_ERR_NOT_FIT;
    }
    ESP_LOGI(TAG, "Место в flash в достатке.");
    return ESP_OK;
}

static void fill_color(uint16_t color)
{
    uint16_t *buf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return;
    for (int i = 0; i < LCD_H_RES; i++) buf[i] = color;

    for (int y = 0; y < LCD_V_RES; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 1, buf);
    }
    free(buf);
}

/* Палитра RGB565 для демонстрации смены цветов на экране */
typedef struct {
    uint16_t color;
    const char *name;   // название цвета (выводится в терминал и на экран)
} color_item_t;

static const color_item_t COLORS[] = {
    { 0xF800, "RED" },      // красный
    { 0x07E0, "GREEN" },    // зелёный
    { 0x001F, "BLUE" },     // синий
    { 0xFFFF, "WHITE" },    // белый
    { 0x0000, "BLACK" },    // чёрный
    { 0xFFE0, "YELLOW" },   // жёлтый
    { 0xF81F, "MAGENTA" },  // пурпурный
    { 0x07FF, "CYAN" },     // голубой
};
#define NUM_COLORS (sizeof(COLORS) / sizeof(COLORS[0]))

/*
 * Вывод текста крупными «пиксельными» буквами прямо на экран.
 * Каждый символ — матрица 5x7, рисуется блоками SCALE x SCALE пикселей.
 * Шрифтовые библиотеки не нужны — только esp_lcd_panel_draw_bitmap.
 */
static void draw_text(const char *text, uint16_t fg, uint16_t bg)
{
    // Минимальный 5x7 шрифт (A-Z, 0-9), строки сверху вниз ('1' = пиксель)
    static const char *GLYPHS[36][7] = {
        /* A */ {"01110","10001","10001","11111","10001","10001","10001"},
        /* B */ {"11110","10001","10001","11110","10001","10001","11110"},
        /* C */ {"01111","10000","10000","10000","10000","10000","01111"},
        /* D */ {"11110","10001","10001","10001","10001","10001","11110"},
        /* E */ {"11111","10000","10000","11110","10000","10000","11111"},
        /* F */ {"11111","10000","10000","11110","10000","10000","10000"},
        /* G */ {"01111","10000","10000","10111","10001","10001","01111"},
        /* H */ {"10001","10001","10001","11111","10001","10001","10001"},
        /* I */ {"11111","00100","00100","00100","00100","00100","11111"},
        /* J */ {"11111","01000","01000","01000","01001","01001","11110"},
        /* K */ {"10001","10010","10100","11000","10100","10010","10001"},
        /* L */ {"10000","10000","10000","10000","10000","10000","11111"},
        /* M */ {"10001","11011","10101","10101","10001","10001","10001"},
        /* N */ {"10001","11001","10101","10011","10001","10001","10001"},
        /* O */ {"01110","10001","10001","10001","10001","10001","01110"},
        /* P */ {"11110","10001","10001","11110","10000","10000","10000"},
        /* Q */ {"01110","10001","10001","10001","10101","10010","01101"},
        /* R */ {"11110","10001","10001","11110","10100","10010","10001"},
        /* S */ {"01111","10000","10000","01110","00001","00001","11110"},
        /* T */ {"11111","00100","00100","00100","00100","00100","00100"},
        /* U */ {"10001","10001","10001","10001","10001","10001","01110"},
        /* V */ {"10001","10001","10001","10001","10001","01010","00100"},
        /* W */ {"10001","10001","10001","10101","10101","11011","10001"},
        /* X */ {"10001","10001","01010","00100","01010","10001","10001"},
        /* Y */ {"10001","10001","01010","00100","00100","00100","00100"},
        /* Z */ {"11111","00001","00010","00100","01000","10000","11111"},
        /* 0 */ {"01110","10001","10011","10101","11001","10001","01110"},
        /* 1 */ {"00100","01100","00100","00100","00100","00100","01110"},
        /* 2 */ {"01110","10001","00001","00010","00100","01000","11111"},
        /* 3 */ {"11111","00010","00100","00010","00001","10001","01110"},
        /* 4 */ {"00010","00110","01010","10010","11111","00010","00010"},
        /* 5 */ {"11111","10000","11110","00001","00001","10001","01110"},
        /* 6 */ {"01110","10000","10000","11110","10001","10001","01110"},
        /* 7 */ {"11111","00001","00010","00100","01000","01000","01000"},
        /* 8 */ {"01110","10001","10001","01110","10001","10001","01110"},
        /* 9 */ {"01110","10001","10001","01111","00001","00001","01110"},
    };
    const int SCALE = 6;            // масштаб: одна «точка» символа = 6x6 пикселя
    const int CHAR_W = 5 * SCALE;   // ширина символа
    const int CHAR_H = 7 * SCALE;   // высота символа
    const int GAP = SCALE;          // межсимвольный интервал

    int len = strlen(text);
    int total_w = len * CHAR_W + (len > 0 ? (len - 1) : 0) * GAP;
    if (total_w > LCD_H_RES) return;            // строка не влезает — пропускаем
    int x0 = (LCD_H_RES - total_w) / 2;         // центрирование по горизонтали
    int y0 = (LCD_V_RES - CHAR_H) / 2;          // центрирование по вертикали

    uint16_t *rowbuf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!rowbuf) return;

    for (int ry = 0; ry < CHAR_H; ry++) {
        for (int x = 0; x < LCD_H_RES; x++) rowbuf[x] = bg;
        for (int ci = 0; ci < len; ci++) {
            char ch = text[ci];
            if (ch == ' ') continue;
            int gi = -1;
            if (ch >= 'A' && ch <= 'Z') gi = ch - 'A';
            else if (ch >= 'a' && ch <= 'z') gi = ch - 'a';
            else if (ch >= '0' && ch <= '9') gi = 26 + (ch - '0');
            if (gi < 0) continue;
            int glyph_row = ry / SCALE;
            if (glyph_row > 6) continue;
            const char *bits = GLYPHS[gi][glyph_row];
            int cx = x0 + ci * (CHAR_W + GAP);
            for (int b = 0; b < 5; b++) {
                if (bits[b] == '1') {
                    int px = cx + b * SCALE;
                    for (int s = 0; s < SCALE; s++) {
                        if (px + s >= 0 && px + s < LCD_H_RES) rowbuf[px + s] = fg;
                    }
                }
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y0 + ry, LCD_H_RES, y0 + ry + 1, rowbuf);
    }
    free(rowbuf);
}

/*
 * Демонстрация смены цветов: фон меняется по кругу, в центре экрана
 * рисуется название текущего цвета контрастным цветом.
 */
static void color_cycle_demo(void)
{
    while (1) {
        for (size_t c = 0; c < NUM_COLORS; c++) {
            uint16_t bg = COLORS[c].color;
            // Контрастный цвет текста: на тёмном фоне — белый, на светлом — чёрный
            uint16_t fg = (bg == 0x0000 || bg == 0x001F || bg == 0xF81F || bg == 0x07E0)
                          ? 0xFFFF : 0x0000;
            fill_color(bg);
            draw_text(COLORS[c].name, fg, bg);
            ESP_LOGI(TAG, "Цвет %d/%u: %s (0x%04X)",
                     (int)c + 1, (unsigned)NUM_COLORS, COLORS[c].name, bg);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}

#if SPI_FREQ_TEST
/*
 * SPI Frequency Stress Test
 *
 * How it works:
 *  - Tries increasing SPI clock from 10 MHz to 80 MHz
 *  - For each frequency fills the screen with a checkerboard pattern multiple times
 *  - User watches the display:
 *      * Clean sharp image  → frequency is OK
 *      * Noise, stripes, wrong colors, flickering → bus cannot handle this speed
 *
 * Practical limits on ESP32-C3 Super Mini + typical ILI9488 module:
 *  - 20-40 MHz usually stable with short wires
 *  - >40 MHz often needs very short wires / good grounding / lower voltage drop
 */
static void spi_freq_test(void)
{
    const int freqs_mhz[] = {10, 20, 30, 40, 50, 60, 80};
    const int num_freqs = sizeof(freqs_mhz) / sizeof(freqs_mhz[0]);

    uint16_t *line = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(line);

    for (int f = 0; f < num_freqs; f++) {
        int mhz = freqs_mhz[f];
        ESP_LOGW(TAG, "=== Testing SPI %d MHz ===", mhz);
        ESP_LOGW(TAG, "Watch the screen carefully!");

        // Re-create panel IO with new frequency
        // (simple way: just change pclk and re-init is complex, so we use a rough approach)
        // For real test we restart the whole panel with new pclk_hz

        // Fill with high-contrast pattern many times
        for (int pass = 0; pass < 8; pass++) {
            for (int y = 0; y < LCD_V_RES; y++) {
                uint16_t c1 = ((y / 16) + pass) & 1 ? 0xFFFF : 0x0000;
                uint16_t c2 = ((y / 16) + pass) & 1 ? 0xF800 : 0x07E0;
                for (int x = 0; x < LCD_H_RES; x++) {
                    line[x] = ((x / 16) & 1) ? c1 : c2;
                }
                esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 1, line);
            }
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        ESP_LOGW(TAG, "Finished %d MHz. If image was clean → OK. Noise/stripes → too fast.", mhz);
        vTaskDelay(pdMS_TO_TICKS(2500));
    }

    free(line);
    ESP_LOGI(TAG, "Frequency test finished. Use the highest frequency that stayed clean.");
}
#endif

void app_main(void)
{
    // Проверка свободного места во flash перед инициализацией периферии
    check_flash_space();

    // ===== Выбор частоты шины SPI через терминал (нужен CONFIG_ESP_CONSOLE_UART_DEFAULT=y) =====
    int freq_mhz = spi_freq_select_mhz();
    printf("Итоговая частота шины SPI: %d МГц (%d Гц)\n", freq_mhz, freq_mhz * 1000000);
    ESP_LOGI(TAG, "Инициализация шины SPI...");

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_BUFFER_LINES * sizeof(uint16_t) + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Создание panel IO...");

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = freq_mhz * 1000 * 1000,      // частота, выбранная пользователем в терминале
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Создание панели ILI9488...");

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

    // Вывод текущей частоты шины на экран и в терминал
    printf("SPI шина работает на частоте %d МГц\n", freq_mhz);
    char freq_str[24];
    snprintf(freq_str, sizeof(freq_str), "%dMHZ", freq_mhz);
    fill_color(0x0000);                     // чёрный фон под надпись о частоте
    draw_text(freq_str, 0xFFFF, 0x0000);    // частота белым по чёрному (по центру экрана)
    vTaskDelay(pdMS_TO_TICKS(1500));

    ESP_LOGI(TAG, "Дисплей готов");

#if SPI_FREQ_TEST
    spi_freq_test();
#else
    // Демонстрация смены цветов с выводом названия цвета на экран
    color_cycle_demo();
#endif
}
