/*
 * ESP-IDF 6.0.3 example: ILI9488 SPI display on ESP32-C3 Super Mini
 *
 * Pinout (recommended):
 *   CS   -> GPIO5
 *   RST  -> GPIO0
 *   DC   -> GPIO1
 *   MOSI -> GPIO4
 *   SCK  -> GPIO2
 *   MISO -> not connected
 *   BL   -> 3.3V (always on)
 */

#include <stdio.h>
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

#define PIN_NUM_SCLK        2       // SCK
#define PIN_NUM_MOSI        4       // SDI / MOSI
#define PIN_NUM_MISO        -1      // not connected
#define PIN_NUM_LCD_CS      5
#define PIN_NUM_LCD_DC      1
#define PIN_NUM_LCD_RST     0

// Resolution
#define LCD_H_RES           320
#define LCD_V_RES           480

// Buffer for 16->18 bit conversion (at least ~1/10 of screen)
#define LCD_BUFFER_LINES    40
#define LCD_BUFFER_SIZE     (LCD_H_RES * LCD_BUFFER_LINES)

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

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,          // 40 MHz (reduce to 20-30 if artifacts)
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Creating ILI9488 panel...");

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 18,                 // REQUIRED for SPI ILI9488
    };

    // buffer_size is mandatory for SPI (color conversion)
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(io_handle, &panel_config, LCD_BUFFER_SIZE, &panel_handle));

    ESP_LOGI(TAG, "Reset & init panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Orientation / mirroring (adjust if needed for your module)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    // Turn display on
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Display ready! Filling colors...");

    // ===== Color fill test =====
    uint16_t *color_buf = heap_caps_malloc(LCD_H_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(color_buf);

    const uint16_t colors[] = {
        0xF800, // Red
        0x07E0, // Green
        0x001F, // Blue
        0xFFFF, // White
        0x0000, // Black
        0xFFE0, // Yellow
        0xF81F, // Magenta
        0x07FF, // Cyan
    };

    while (1) {
        for (int c = 0; c < 8; c++) {
            for (int i = 0; i < LCD_H_RES; i++) {
                color_buf[i] = colors[c];
            }

            // Fill screen line by line
            for (int y = 0; y < LCD_V_RES; y++) {
                esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + 1, color_buf);
            }

            ESP_LOGI(TAG, "Color %d", c);
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}
