/*
 * ESP32-C3 + ILI9488 + LVGL 9
 * Красивое демо с круглыми шрифтами и кириллицей
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9488.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "ui.h"

static const char *TAG = "MAIN";

// Распиновка
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

void app_main(void)
{
    ESP_LOGI(TAG, "Запуск демо ILI9488 + LVGL 9...");

    // ----- Шина SPI -----
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_BUFFER_LINES * sizeof(uint16_t) + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // ----- Интерфейс панели (Panel IO) -----
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,     // 40 МГц — хороший баланс скорости и надёжности
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // ----- Панель ILI9488 -----
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

    // ----- Порт LVGL -----
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 30,          // частичный буфер
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,               // у C3 обычно нет PSRAM
        }
    };

    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    assert(disp);

    // Создаём UI под блокировкой LVGL
    if (lvgl_port_lock(0)) {
        ui_init();
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "UI готов. Наслаждайтесь плавной кириллицей!");

    // Основной цикл ничего не делает — LVGL работает в собственной задаче
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
