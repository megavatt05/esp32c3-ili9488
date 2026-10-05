/*
 * Меню выбора частоты SPI-шины через терминал (UART0).
 * Реализация: печать списка + uart_input_line() с таймаутом
 * (надёжный приём напрямую из UART, без зависимости от fgets/stdin).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "spi_freq_menu.h"
#include "uart_input.h"

static const char *TAG_FREQ = "SPI_MENU";

#define FREQ_INPUT_TIMEOUT_MS 30000   // ждём выбор пользователя 30 секунд

int spi_freq_select_mhz(void)
{
    /* Стандартные частоты по порядку: 1 => 10 МГц, 2 => 20 МГц и т.д. */
    const int freqs[] = SPI_FREQ_LIST;
    const int n = sizeof(freqs) / sizeof(freqs[0]);
    const int min_mhz = 1;      // нижняя граница произвольного ввода
    const int max_mhz = 80;     // верхняя граница (GPIO-матрица C3 ~80 МГц на практике)
    const int default_mhz = 40; // значение по умолчанию при таймауте/ошибке

    while (1) {
        printf("\n================ Выбор частоты шины SPI ================\n");
        printf("Стандартные значения (введите номер):\n");
        for (int i = 0; i < n; i++) {
            printf("  %d) %d МГц\n", i + 1, freqs[i]);
        }
        printf("Номер пункта: 1, 2, 3 ... %d\n", n);
        printf("Или явная частота с суффиксом M, например 45M (%d..%d МГц).\n", min_mhz, max_mhz);
        printf("(без ответа за %d c будет выбрано %d МГц)\n> ",
               FREQ_INPUT_TIMEOUT_MS / 1000, default_mhz);
        fflush(stdout);

        char line[32];
        int rlen = uart_input_line(line, sizeof(line), FREQ_INPUT_TIMEOUT_MS);
        if (rlen < 0) {
            // Терминал молчит (не подключён или никто не вводит) —
            // берём значение по умолчанию, чтобы прошивка не зависела.
            ESP_LOGW(TAG_FREQ, "Ввод не получен за %d мс — частота по умолчанию %d МГц",
                     FREQ_INPUT_TIMEOUT_MS, default_mhz);
            return default_mhz;
        }
        if (line[0] == 0) continue;         // пустой ввод — переспросить

        char *endp = NULL;
        long val = strtol(line, &endp, 10);

        // Вариант 1: явная частота с суффиксом "M" (например "45M")
        if (endp && (*endp == 'M' || *endp == 'm') && *(endp + 1) == 0) {
            if (val >= min_mhz && val <= max_mhz) {
                printf("Выбрано: %ld МГц\n", val);
                return (int)val;
            }
            printf("Частота вне диапазона %d..%d МГц.\n", min_mhz, max_mhz);
            continue;
        }

        // Вариант 2: просто число в мегагерцах (например "45")
        if (*endp == 0 && val >= min_mhz && val <= max_mhz && val > n) {
            printf("Выбрано: %ld МГц (прямое значение)\n", val);
            return (int)val;
        }

        // Вариант 3: номер пункта из списка (1, 2, 3 ...)
        if (*endp == 0 && val >= 1 && val <= n) {
            printf("Выбрано: пункт %ld -> %d МГц\n", val, freqs[val - 1]);
            return freqs[val - 1];
        }

        printf("Некорректный ввод '%s'. Введите номер 1..%d или частоту вида 45M.\n",
               line, n);
    }
}
