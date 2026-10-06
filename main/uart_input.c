/*
 * Приём строк с консольного UART (режим RAW).
 *
 * КЛЮЧЕВОЕ РЕШЕНИЕ (почему раньше «ничего нельзя было написать»):
 *   В IDF 5.x/6.x стандартный вывод консоли идёт через драйвер UART,
 *   а VFS-слой stdin остаётся НЕинициализированным. Поэтому scanf()/getchar()
 *   возвращают EOF мгновенно — меню не получало ввод вообще.
 *   Теперь читаем байты НАПРЯМУЮ из RX-кольца драйвера (uart_read_bytes),
 *   минуя VFS/stdin. Работает при любом CONFIG_ESP_CONSOLE_*.
 *
 *   Enter на Windows/idf.py monitor шлёт '\r', а не '\n' — принимаем оба.
 *
 * Документация:
 *   https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c3/api-reference/peripherals/uart.html
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "uart_input.h"

/*
 * VFS-привязка stdin к драйверу UART. В IDF 6.x API переехал в
 * driver/uart_vfs.h (uart_vfs_dev_use_driver); в IDF 5.x — esp_vfs_dev.h.
 * Без этой привязки scanf()/getchar() читают из «сырого» VFS и мгновенно
 * возвращают EOF — одна из причин, почему раньше нельзя было ничего ввести.
 */
#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"
#define UART_VFS_NEW_API 1
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"
#define UART_VFS_NEW_API 0
#else
#define UART_VFS_NEW_API -1
#endif

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

    /* NVS обязателен: без него часть подсистем консоли/Wi-Fi ведёт себя нестабильно */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_err_t derr = uart_driver_install(UART_NUM, 1024, 0, 0, NULL, 0);
    if (derr == ESP_OK) {
        ESP_LOGI(TAG_UART_IN, "Драйвер UART%d установлен", (int)UART_NUM);
    } else if (derr == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_UART_IN, "Драйвер UART%d уже установлен — переключаю в RAW-режим", (int)UART_NUM);
    } else {
        ESP_LOGE(TAG_UART_IN, "uart_driver_install: %s", esp_err_to_name(derr));
    }

#if UART_VFS_NEW_API == 1
    /* IDF 6.x: привязываем stdin/stdout к драйверу и переводим CR в LF */
    uart_vfs_dev_use_driver(UART_NUM);
    uart_vfs_dev_port_set_rx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CR);
    uart_vfs_dev_port_set_tx_line_endings(UART_NUM, ESP_LINE_ENDINGS_CRLF);
    ESP_LOGI(TAG_UART_IN, "stdin привязан к драйверу UART%d (uart_vfs_dev)", (int)UART_NUM);
#elif UART_VFS_NEW_API == 0
    esp_err_t verr = esp_vfs_dev_uart_use_driver(UART_NUM);
    if (verr != ESP_OK) {
        ESP_LOGE(TAG_UART_IN, "esp_vfs_dev_uart_use_driver: %s", esp_err_to_name(verr));
    } else {
        ESP_LOGI(TAG_UART_IN, "stdin привязан к драйверу UART%d (esp_vfs_dev)", (int)UART_NUM);
    }
#else
    ESP_LOGW(TAG_UART_IN, "VFS-dev заголовок не найден — доступен только RAW-ввод");
#endif

    s_ready = true;
    ESP_LOGI(TAG_UART_IN, "Ввод готов: слушаю UART%d напрямую (GPIO20=RX, GPIO21=TX, 115200). "
             "Если лог идёт через встроенный USB-C3 — включите консоль USB-Serial-JTAG.", (int)UART_NUM);
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
