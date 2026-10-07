# uart-only — стандартный пример UART Echo (ESP-IDF v6.0.3)

Чистая ветка без LVGL, дисплея и Wi-Fi: официальный пример
`examples/peripherals/uart/echo` из ESP-IDF v6.0.3, адаптированный под
esp32c3 с комментариями на русском.

## Что делает
- **UART1** (TX=GPIO4, RX=GPIO5) каждую секунду отправляет тестовую строку;
- **UART2** (TX=GPIO6, RX=GPIO7) принимает её и возвращает обратно эхом;
- консоль/отладка — UART0 (GPIO20/21, 115200).

Для работы примера нужна **перемычка GPIO4→GPIO7 и GPIO5→GPIO6**
(или просто GPIO4→GPIO6/TX2-RX2 по схеме loopback на вашей плате).

## Сборка
```bash
idf.py set-target esp32c3
del sdkconfig          # Windows (rm -rf sdkconfig на Linux)
idf.py fullclean
idf.py build flash monitor
```

## Примечание про ESP-IDF 6.x
Драйвер UART вынесен в отдельный компонент `esp_driver_uart` — он указан в
`REQUIRES` файла `main/CMakeLists.txt` (вместо зонтичного `driver`).
