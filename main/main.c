/*
 * ESP32-C3 — только UART0.
 * Без дисплея, без Wi-Fi, без BT.
 *
 * Приём с клавиатуры терминала (idf.py monitor):
 *   ввод эхируется, Enter завершает строку (CR или LF).
 * Отправка: эхо, ответы на команды, опциональный tick.
 *
 * ESP-IDF 6.x:
 *   #include "driver/uart_vfs.h"
 *   uart_vfs_dev_use_driver() → void
 *   uart_write_bytes(), не uart_write_byte()
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/uart.h"

#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"
#define UART_VFS_HAS_NEW_API 1
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"
#define UART_VFS_HAS_NEW_API 0
#else
#define UART_VFS_HAS_NEW_API -1
#endif

#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM 0
#endif

#define UART_NUM   ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM)
#define UART_BAUD  115200
#define UART_RX_BUF 2048
#define UART_TX_BUF 1024
#define LINE_MAX    128

static const char *TAG = "UART";

static volatile uint32_t s_rx_bytes;
static volatile uint32_t s_tx_bytes;
static volatile uint32_t s_rx_lines;
static volatile bool s_tick_on;

static void uart_send(const char *s)
{
    if (!s) {
        return;
    }
    size_t n = strlen(s);
    int w = uart_write_bytes(UART_NUM, s, n);
    if (w > 0) {
        s_tx_bytes += (uint32_t)w;
    }
}

static void uart_send_crlf(void)
{
    uart_send("\r\n");
}

static void uart_printf_line(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        uart_send(buf);
        uart_send_crlf();
    }
}

static void print_help(void)
{
    uart_send("\r\n=== UART console ESP32-C3 ===\r\n");
    uart_send("  любой текст     эхо строки обратно\r\n");
    uart_send("  help              эта справка\r\n");
    uart_send("  ping              ответ pong\r\n");
    uart_send("  echo <text>       отправить text\r\n");
    uart_send("  hex <text>        показать text в hex\r\n");
    uart_send("  stats             счётчики RX/TX\r\n");
    uart_send("  tick on|off       периодическая отправка\r\n");
    uart_send("  info              параметры UART\r\n");
    uart_send("> ");
}

static void cmd_hex(const char *s)
{
    uart_send("HEX:");
    for (; *s; s++) {
        char tmp[8];
        snprintf(tmp, sizeof(tmp), " %02X", (unsigned char)*s);
        uart_send(tmp);
    }
    uart_send_crlf();
}

static void handle_line(char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t')) {
        line[--n] = 0;
    }
    if (n == 0) {
        uart_send("> ");
        return;
    }

    s_rx_lines++;

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        print_help();
        return;
    }
    if (strcmp(line, "ping") == 0) {
        uart_send("pong\r\n> ");
        return;
    }
    if (strcmp(line, "stats") == 0) {
        uart_printf_line("stats: rx_bytes=%u tx_bytes=%u lines=%u tick=%s",
                         (unsigned)s_rx_bytes, (unsigned)s_tx_bytes,
                         (unsigned)s_rx_lines, s_tick_on ? "on" : "off");
        uart_send("> ");
        return;
    }
    if (strcmp(line, "info") == 0) {
        uart_printf_line("UART%d %d 8N1  console RX=GPIO20 TX=GPIO21 (C3 Super Mini)",
                         (int)UART_NUM, UART_BAUD);
        uart_printf_line("free_heap=%u uptime_ms=%lld",
                         (unsigned)esp_get_free_heap_size(),
                         (long long)(esp_timer_get_time() / 1000));
        uart_send("> ");
        return;
    }
    if (strcmp(line, "tick on") == 0) {
        s_tick_on = true;
        uart_send("tick ON (5s)\r\n> ");
        return;
    }
    if (strcmp(line, "tick off") == 0) {
        s_tick_on = false;
        uart_send("tick OFF\r\n> ");
        return;
    }
    if (strncmp(line, "echo ", 5) == 0) {
        uart_send(line + 5);
        uart_send("\r\n> ");
        return;
    }
    if (strncmp(line, "hex ", 4) == 0) {
        cmd_hex(line + 4);
        uart_send("> ");
        return;
    }

    uart_send("RX: ");
    uart_send(line);
    uart_send("\r\nTX: ");
    uart_send(line);
    uart_send("\r\n> ");
}

static void tick_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_tick_on) {
            continue;
        }
        uart_printf_line("[tick] uptime=%lld ms rx=%u tx=%u",
                         (long long)(esp_timer_get_time() / 1000),
                         (unsigned)s_rx_bytes, (unsigned)s_tx_bytes);
    }
}

static void uart_rx_task(void *arg)
{
    (void)arg;
    char line[LINE_MAX];
    size_t len = 0;

    while (1) {
        uint8_t ch;
        int n = uart_read_bytes(UART_NUM, &ch, 1, pdMS_TO_TICKS(50));
        if (n <= 0) {
            continue;
        }
        s_rx_bytes += (uint32_t)n;

        if (ch == '\r' || ch == '\n') {
            if (len == 0) {
                continue;
            }
            line[len] = 0;
            uart_send("\r\n");
            handle_line(line);
            len = 0;
            continue;
        }
        if (ch == 0x08 || ch == 0x7F) {
            if (len > 0) {
                len--;
                uart_send("\b \b");
            }
            continue;
        }
        if (ch >= 32 && ch < 127) {
            if (len + 1 < sizeof(line)) {
                line[len++] = (char)ch;
                uart_write_bytes(UART_NUM, (const char *)&ch, 1);
                s_tx_bytes++;
            }
        }
    }
}

static void uart_console_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    esp_err_t err = uart_driver_install(UART_NUM, UART_RX_BUF, UART_TX_BUF, 0, NULL, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "UART%d driver already installed", (int)UART_NUM);
    } else {
        ESP_ERROR_CHECK(err);
    }

#if UART_VFS_HAS_NEW_API == 1
    uart_vfs_dev_use_driver(UART_NUM);
    uart_vfs_dev_port_set_rx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CRLF);
#elif UART_VFS_HAS_NEW_API == 0
    (void)esp_vfs_dev_uart_use_driver(UART_NUM);
#endif
}

void app_main(void)
{
    uart_console_init();

    vTaskDelay(pdMS_TO_TICKS(200));
    uart_send("\r\n\r\n**** ESP32-C3 UART-only ****\r\n");
    uart_printf_line("UART%d %d 8N1  RX=GPIO20 TX=GPIO21", (int)UART_NUM, UART_BAUD);
    uart_send("Type on the keyboard, Enter sends the line.\r\n");
    uart_send("Command help — list of commands.\r\n> ");

    xTaskCreate(uart_rx_task, "uart_rx", 4096, NULL, 5, NULL);
    xTaskCreate(tick_task, "uart_tick", 2048, NULL, 3, NULL);
}
