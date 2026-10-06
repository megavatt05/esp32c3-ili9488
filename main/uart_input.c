/*
 * Надёжный опрос UART0-консоли для интерактивного ввода.
 *
 * ESP-IDF 5.3+ / 6.x:
 *  - UART VFS перенесён в esp_driver_uart
 *  - esp_vfs_dev_uart_*  →  uart_vfs_dev_*
 *  - заголовок: driver/uart_vfs.h
 *
 * Документация:
 *  https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32c3/migration-guides/release-5.x/5.3/storage.html
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

// Совместимость версий ESP-IDF:
//  - IDF >= 5.3 / 6.x : driver/uart_vfs.h + uart_vfs_dev_use_driver()
//  - IDF <= 5.2       : esp_vfs_dev.h     + esp_vfs_dev_uart_use_driver()
#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"          // ESP-IDF 5.3+ / 6.x
#define UART_VFS_USE_DRIVER(n)  uart_vfs_dev_use_driver(n)
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"              // ESP-IDF 5.0 – 5.2
#define UART_VFS_USE_DRIVER(n)  esp_vfs_dev_uart_use_driver(n)
#else
#define UART_VFS_USE_DRIVER(n)  ESP_ERR_NOT_SUPPORTED
#endif

#include "esp_log.h"
#include "uart_input.h"

static const char *TAG_UART_IN = "UART_IN";
static bool s_ready = false;

void uart_input_init(void)
{
    if (s_ready) return;

    // Инициализируем драйвер приёма UART0.
    // uart_driver_install() не трогает baud/конфигурацию —
    // настройки ROM-консоли сохраняются (обычно 115200).
    esp_err_t derr = uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    if (derr == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_UART_IN, "Драйвер UART0 уже установлен — используем его");
    } else if (derr != ESP_OK) {
        ESP_LOGE(TAG_UART_IN, "uart_driver_install: %s", esp_err_to_name(derr));
    }

    // Привязать stdin к драйверу UART0
    esp_err_t verr = UART_VFS_USE_DRIVER(0);
    if (verr != ESP_OK) {
        ESP_LOGW(TAG_UART_IN, "uart_vfs_dev_use_driver: %s (используем прямой опрос UART)",
                 esp_err_to_name(verr));
    }

    s_ready = true;
    ESP_LOGI(TAG_UART_IN, "Ввод через UART0 готов");
}

int uart_input_line(char *buf, size_t buf_size, int timeout_ms)
{
    if (!s_ready) uart_input_init();

    size_t len = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);

    while ((int)(deadline - xTaskGetTickCount()) > 0) {
        uint8_t ch;
        int n = uart_read_bytes(UART_NUM_0, &ch, 1, pdMS_TO_TICKS(50));
        if (n <= 0) continue;

        if (ch == '\r') continue;
        if (ch == '\n') {
            if (len == 0) continue;
            buf[len] = 0;
            return (int)len;
        }
        if (ch >= ' ' && ch <= '~') {
            if (len + 1 < buf_size) {
                buf[len++] = (char)ch;
                // Эхо в терминал (правильная функция — uart_write_bytes)
                uart_write_bytes(UART_NUM_0, (const char *)&ch, 1);
            }
        }
    }
    buf[0] = 0;
    return -1;   // таймаут
}
