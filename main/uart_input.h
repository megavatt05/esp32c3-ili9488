/*
 * Надёжный построчный ввод из UART0-консоли с таймаутом.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Инициализировать приём через UART0 (драйвер + привязка stdin).
 * Вызывается автоматически из uart_input_line(), но лучше вызвать
 * один раз в начале app_main(). */
void uart_input_init(void);

/*
 * Прочитать одну строку (до '\n') из UART0.
 *   buf       — буфер под строку ('\0'-терминированная);
 *   buf_size  — размер буфера;
 *   timeout_ms — сколько миллисекунд ждать ввода всего.
 * Возвращает длину строки (>=0) или -1, если за таймаут данных не было.
 */
int uart_input_line(char *buf, size_t buf_size, int timeout_ms);

#ifdef __cplusplus
}
#endif
