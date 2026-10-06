/*
 * Приём строк с консольного UART.
 *
 * Важно для Windows / idf.py monitor:
 *   клавиша Enter шлёт '\r', а не '\n'. Старый код пропускал '\r'
 *   и ждал '\n', поэтому ввод никогда не завершался.
 *
 * Документация:
 *   https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c3/api-reference/storage/vfs.html
 *   uart_vfs_dev_use_driver() возвращает void.
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

#include "esp_log.h"
#include "uart_input.h"

#ifndef CONFIG_ESP_CONSOLE_UART_NUM
#define CONFIG_ESP_CONSOLE_UART_NUM 0
#endif

#define UART_NUM  ((uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM)

static const char *TAG_UART_IN = "UART_IN";
static bool s_ready = false;

/* Незавершённая строка между вызовами uart_input_line() */
static char s_acc[32];
static size_t s_acc_len = 0;

void uart_input_init(void)
{
    if (s_ready) {
        return;
    }

    esp_err_t derr = uart_driver_install(UART_NUM, 1024, 0, 0, NULL, 0);
    if (derr == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_UART_IN, "Драйвер UART%d уже установлен", (int)UART_NUM);
    } else if (derr != ESP_OK) {
        ESP_LOGE(TAG_UART_IN, "uart_driver_install: %s", esp_err_to_name(derr));
    }

#if UART_VFS_HAS_NEW_API == 1
    uart_vfs_dev_use_driver(UART_NUM);
    uart_vfs_dev_port_set_rx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CRLF);
#elif UART_VFS_HAS_NEW_API == 0
    (void)esp_vfs_dev_uart_use_driver(UART_NUM);
#endif

    s_ready = true;
    ESP_LOGI(TAG_UART_IN, "Ввод через UART%d готов (Enter = CR или LF)", (int)UART_NUM);
}

int uart_input_line(char *buf, size_t buf_size, int timeout_ms)
{
    if (!s_ready) {
        uart_input_init();
    }
    if (!buf || buf_size < 2) {
        return -1;
    }

    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while ((int)(deadline - xTaskGetTickCount()) > 0) {
        uint8_t ch;
        int n = uart_read_bytes(UART_NUM, &ch, 1, pdMS_TO_TICKS(20));
        if (n <= 0) {
            continue;
        }

        /* CR и LF — конец строки. Пара CR+LF не даёт пустую вторую строку. */
        if (ch == '\r' || ch == '\n') {
            if (s_acc_len == 0) {
                continue;
            }
            if (s_acc_len >= buf_size) {
                s_acc_len = buf_size - 1;
            }
            memcpy(buf, s_acc, s_acc_len);
            buf[s_acc_len] = 0;
            int out = (int)s_acc_len;
            s_acc_len = 0;
            const char crlf[] = "\r\n";
            uart_write_bytes(UART_NUM, crlf, 2);
            return out;
        }

        if (ch == 0x08 || ch == 0x7F) { /* Backspace */
            if (s_acc_len > 0) {
                s_acc_len--;
                uart_write_bytes(UART_NUM, "\b \b", 3);
            }
            continue;
        }

        if (ch >= ' ' && ch <= '~') {
            if (s_acc_len + 1 < sizeof(s_acc)) {
                s_acc[s_acc_len++] = (char)ch;
                uart_write_bytes(UART_NUM, (const char *)&ch, 1);
            }
        }
    }

    buf[0] = 0;
    return -1;
}
