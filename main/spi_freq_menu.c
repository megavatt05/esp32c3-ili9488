/*
 * Меню частоты SPI. Ввод: номер пункта 1..8, либо 45 / 45M.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "spi_freq_menu.h"
#include "uart_input.h"

static const char *TAG_FREQ = "SPI_MENU";

#define FREQ_INPUT_TIMEOUT_MS 45000
#define MIN_MHZ 1
#define MAX_MHZ 80
#define DEFAULT_MHZ 40

int spi_freq_parse_mhz(const char *line)
{
    if (!line || line[0] == 0) {
        return 0;
    }

    const int freqs[] = SPI_FREQ_LIST;
    const int n = (int)(sizeof(freqs) / sizeof(freqs[0]));

    char *endp = NULL;
    long val = strtol(line, &endp, 10);
    if (endp == line) {
        return 0;
    }
    while (*endp == ' ' || *endp == '\t') {
        endp++;
    }

    if (*endp == 'M' || *endp == 'm') {
        endp++;
        if (*endp == 'H' || *endp == 'h') {
            endp++;
            if (*endp == 'Z' || *endp == 'z') {
                endp++;
            }
        }
        if (*endp == 0 && val >= MIN_MHZ && val <= MAX_MHZ) {
            return (int)val;
        }
        return 0;
    }

    if (*endp != 0) {
        return 0;
    }

    /* Число 1..N — пункт меню. Число > N — частота в МГц. */
    if (val >= 1 && val <= n) {
        return freqs[val - 1];
    }
    if (val >= MIN_MHZ && val <= MAX_MHZ) {
        return (int)val;
    }
    return 0;
}

int spi_freq_select_mhz(void)
{
    const int freqs[] = SPI_FREQ_LIST;
    const int n = (int)(sizeof(freqs) / sizeof(freqs[0]));

    while (1) {
        printf("\n================ Выбор частоты шины SPI ================\n");
        printf("Введите номер пункта и нажмите Enter:\n");
        for (int i = 0; i < n; i++) {
            printf("  %d) %d МГц\n", i + 1, freqs[i]);
        }
        printf("Или частоту: 45  либо  45M  (%d..%d МГц)\n", MIN_MHZ, MAX_MHZ);
        printf("На Windows Enter = CR — это нормально, ввод принимается.\n");
        printf("(без ответа за %d с будет %d МГц; потом можно сменить в любой момент)\n> ",
               FREQ_INPUT_TIMEOUT_MS / 1000, DEFAULT_MHZ);
        fflush(stdout);

        char line[32];
        int rlen = uart_input_line(line, sizeof(line), FREQ_INPUT_TIMEOUT_MS);
        if (rlen < 0) {
            ESP_LOGW(TAG_FREQ, "Ввод не получен за %d мс — %d МГц. Дальше можно ввести частоту в любой момент.",
                     FREQ_INPUT_TIMEOUT_MS, DEFAULT_MHZ);
            return DEFAULT_MHZ;
        }

        int mhz = spi_freq_parse_mhz(line);
        if (mhz > 0) {
            printf("Выбрано: %d МГц\n", mhz);
            return mhz;
        }
        printf("Некорректный ввод '%s'. Пример: 2  или  40M\n", line);
    }
}
