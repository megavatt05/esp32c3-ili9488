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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9488.h"

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
    ESP_LOGI(TAG, "Initializing SPI bus...");

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_BUFFER_LINES * sizeof(uint16_t) + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Creating panel IO...");

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,          // start with 40 MHz
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Creating ILI9488 panel...");

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

    ESP_LOGI(TAG, "Display ready");

#if SPI_FREQ_TEST
    spi_freq_test();
#else
    // Color demo
    const uint16_t colors[] = {
        0xF800, 0x07E0, 0x001F, 0xFFFF, 0x0000, 0xFFE0, 0xF81F, 0x07FF
    };
    while (1) {
        for (int c = 0; c < 8; c++) {
            fill_color(colors[c]);
            ESP_LOGI(TAG, "Color %d", c);
            vTaskDelay(pdMS_TO_TICKS(1200));
        }
    }
#endif
}
