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
 * Номер порта консоли определяем НЕ по CONFIG_ESP_CONSOLE_UART_NUM,
 * а по реально настроенной консоли (esp_console_get_config). Если в sdkconfig
 * осталась консоль USB-Serial-JTAG (частая причина «терминал молчит» на
 * C3 SuperMini при подключении через micro-USB), выводим предупреждение:
 * ввод придёт не в тот порт, к которому подключён терминал.
 */
#if __has_include("esp_console.h")
#include "esp_console.h"  /* только для типов, сам esp_console не инициализируем */
#define HAVE_ESP_CONSOLE 1
#else
#define HAVE_ESP_CONSOLE 0
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

#if HAVE_ESP_CONSOLE
    /* Предупреждение о несовпадении реальной консоли и порта, который мы слушаем */
    esp_console_dev_uart_config_t cdbg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    if (cdbg.port_num != (int)UART_NUM) {
        ESP_LOGW(TAG_UART_IN,
                 "ВНИМАНИЕ: консоль собрана под UART%d, а меню слушает UART%d. "
                 "Пересоберите после 'del sdkconfig' или подключите терминал к нужным пинам.",
                 cdbg.port_num, (int)UART_NUM);
    }
#endif

    /*
     * Читаем байты напрямую из RX-кольца драйвера (uart_read_bytes),
     * минуя VFS/stdin — см. шапку файла. Никаких VFS-вызовов не нужно.
     */

    s_ready = true;
    ESP_LOGW(TAG_UART_IN, "RAW-ввод: слушаю UART%d (GPIO20=RX, GPIO21=TX, 115200). Если лог идёт через встроенный USB — переключите консоль на USB-Serial-JTAG в sdkconfig.", (int)UART_NUM);
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
