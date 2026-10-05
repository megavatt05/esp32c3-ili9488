/*
 * Надёжный опрос UART0-консоли для интерактивного ввода.
 *
 * Почему нельзя использовать fgets(stdin) «как есть»:
 *  - в ESP-IDF начиная с v5.x стандартный ввод по умолчанию НЕ привязан
 *    к UART-консоли (CONFIG_ESP_CONSOLE_SECONDARY_NONE=y), поэтому fgets
 *    сразу возвращает EOF и код молча берёт частоту по умолчанию;
 *  - даже при включённом secondary-input fgets без таймаута может
 *    «зависнуть» намертво, если терминал не подключён.
 *
 * Здесь: прямой приём байтов через driver/uart + VFS-обёртка
 * esp_vfs_dev_uart_use_driver(0), чтобы getline()/stdin тоже работали.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
// Совместимость версий ESP-IDF:
//  - IDF <= 5.3: esp_vfs_dev_uart_use_driver() объявлена в esp_vfs_dev.h (компонент vfs);
//  - IDF >= 6.0: заголовок esp_vfs_dev.h УДАЛЁН, функция перенесена в driver/uart_vfs.h.
#if __has_include("driver/uart_vfs.h")
#include "driver/uart_vfs.h"   // ESP-IDF 6.x
#elif __has_include("esp_vfs_dev.h")
#include "esp_vfs_dev.h"       // ESP-IDF 5.x
#endif
#include "esp_log.h"
#include "uart_input.h"

static const char *TAG_UART_IN = "UART_IN";
static bool s_ready = false;

void uart_input_init(void)
{
    if (s_ready) return;

    // Инициализируем ДРАЙВЕР приёма UART0.
    // ВАЖНО: uart_driver_install() НЕ трогает baud/конфигурацию —
    // настройки, заданные ROM-консолью на этапе загрузки, сохраняются,
    // поэтому лог и ввод идут на той же скорости 115200.
    esp_err_t derr = uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    if (derr == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG_UART_IN, "Драйвер UART0 уже установлен — используем его");
    } else if (derr != ESP_OK) {
        ESP_LOGE(TAG_UART_IN, "uart_driver_install: %s", esp_err_to_name(derr));
    }

    // Привязать stdin к драйверу UART0 (а не к ROM-функциям).
    // Насколько известно из документации ESP-IDF, вызов допустим в
    // любое время; делаем best-effort: при неудаче полагаемся на
    // прямой опрос UART через uart_read_bytes().
    esp_err_t verr = esp_vfs_dev_uart_use_driver(0);
    if (verr != ESP_OK) {
        ESP_LOGW(TAG_UART_IN, "esp_vfs_dev_uart_use_driver: %s (используем прямой опрос UART)",
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
        int n = uart_read_bytes(0, &ch, 1, pdMS_TO_TICKS(50));
        if (n <= 0) continue;

        // Игнорируем символы управления и перевод строк между «строками»
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (len == 0) continue;      // пустая строка — ждём дальше
            buf[len] = 0;
            return (int)len;             // полная строка получена
        }
        if (ch >= ' ' && ch <= '~') {    // только печатаемые ASCII
            if (len + 1 < buf_size) {
                buf[len++] = (char)ch;
                uart_write_byte(0, ch);  // эхо набранного в терминал
            }
        }
    }
    buf[0] = 0;
    return -1;                           // таймаут — данных не было
}
