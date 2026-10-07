/*
 * SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Пример UART Echo — стандартный пример ESP-IDF v6.0.3
 * examples/peripherals/uart/echo
 *
 * Демонстрирует обмен данными между двумя портами UART, соединёнными перемычкой:
 *   - UART1 (TX=GPIO4, RX=GPIO5) — основной канал, шлёт тестовую строку;
 *   - UART2 (RX=GPIO6, TX=GPIO7) — канал-эхо, возвращает прочитанное обратно.
 *
 * Отладочный вывод идёт через консоль (UART0 / USB-JTAG).
 *
 * Наблюдение: idf.py set-target esp32c3 && idf.py build flash monitor
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define ECHO_TEST_TXD  (GPIO_NUM_4)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (GPIO_NUM_6)
#define ECHO_TEST_CTS  (GPIO_NUM_7)

#define BUF_SIZE (1024)

/// Задача основного канала: отправляет тестовую строку в UART1 и читает ответ
static void echo_task(void *arg)
{
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, ECHO_TEST_TXD, ECHO_TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Буфер для принятых данных
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    const char *str = "ESP32 test string\n";

    while (1) {
        // Отправка данных в UART1
        ESP_ERROR_CHECK(uart_write_bytes(UART_NUM_1, str, strlen(str)));
        printf("Sent: %s", str);
        // Чтение ответа (эха) из UART1
        int len = uart_read_bytes(UART_NUM_1, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            printf("Echo received from UART1: %s", (char *) data);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    free(data);
    vTaskDelete(NULL);
}

/// Задача канала-эхо: читает из UART2 и возвращает прочитанное обратно
static void echo_loopback_task(void *arg)
{
    uart_config_t uart_config = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, ECHO_TEST_RTS, ECHO_TEST_CTS, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        uart_write_bytes(UART_NUM_2, (const char *) data, len);
    }
    free(data);
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Создаём две задачи: отправитель (UART1) и эхо-ответчик (UART2)
    xTaskCreate(echo_task, "uart_echo_task", 2048, NULL, 10, NULL);
    xTaskCreate(echo_loopback_task, "uart_loopback_task", 2048, NULL, 10, NULL);
}
