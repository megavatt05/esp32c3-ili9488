/*
 * Выбор частоты SPI для ILI9488 через терминал.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SPI_FREQ_LIST { 10, 20, 30, 40, 50, 60, 70, 80 }

int spi_freq_select_mhz(void);

/* Разобрать строку ввода. Возвращает МГц или 0, если некорректно. */
int spi_freq_parse_mhz(const char *line);

#ifdef __cplusplus
}
#endif
