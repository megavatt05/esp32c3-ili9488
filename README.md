# uart-only — ESP32-C3 UART TX/RX

Ветка **без дисплея и без Wi-Fi**. Только полный UART: приём, отправка, ввод с клавиатуры в `idf.py monitor`.

## Железо

ESP32-C3 Super Mini, USB-UART:

| Сигнал | GPIO |
|--------|------|
| UART0 RX | 20 |
| UART0 TX | 21 |
| GND | GND |

Скорость: **115200 8N1**.

## Сборка

```powershell
git fetch origin
git checkout uart-only
idf.py fullclean
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor
```

В мониторе печатай текст и нажимай **Enter** (Windows CR поддерживается).

## Команды

| Ввод | Что делает |
|------|-----------|
| любой текст | эхо обратно в терминал |
| `help` | список команд |
| `ping` | ответ `pong` |
| `echo привет` | отправить строку |
| `hex привет` | то же в hex |
| `stats` | счётчики RX/TX |
| `tick on` / `tick off` | периодическая отправка статуса |
| `info` | UART, baud, пины |
