/*
 * Построчный ввод из консольного UART с таймаутом.
 * Незавершённая строка сохраняется между вызовами (можно опрашивать короткими таймаутами).
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_input_init(void);

/*
 * Прочитать одну строку из UART.
 * Конец строки: '\n' или '\r' (Windows / idf.py monitor).
 * Возвращает длину (>=1) или -1, если за timeout_ms полной строки не было.
 * Незавершённый ввод не теряется — следующий вызов его продолжит.
 */
int uart_input_line(char *buf, size_t buf_size, int timeout_ms);

#ifdef __cplusplus
}
#endif
