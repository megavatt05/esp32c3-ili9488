# ESP32C3-DGW-SA1 — Display Gateway SoftAP Edition

**Инженерное название проекта:** `ESP32C3-DGW-SA1` (Display GateWay — SoftAP, версия 1)

Прошивка для ESP32-C3 + дисплея ILI9488 (SPI, 320×480) на LVGL 9 с кириллическими
шрифтами и встроенной точкой доступа Wi-Fi. Телефон подключается к AP устройства —
страница управления открывается автоматически (captive portal).

## Возможности
- 🖥 UI на LVGL 9: фото-фон, карточки, шрифты Inter/Roboto/Montserrat/NotoSans с кириллицей
- 📶 Автоматический старт SoftAP (WPA2) при включении питания
- 🌐 Веб-страница управления: цвет фона экрана, счётчик клиентов (HTTP, порт 80)
- 🔗 Автооткрытие страницы: DNS-перехват (порт 53) + ответы на проверки порталов Android/iOS
- 🏷 mDNS: интерфейс доступен по `http://dgw-sa1.local`
- 🖼 Баннер «SOFTAP ONLINE» на экране при подключении первого клиента

## Точка доступа
| Параметр | Значение |
|---|---|
| SSID | `DGW-SA1-<XXXX>` (последние 2 байта MAC) |
| Пароль | `displaygw` |
| IP устройства | `192.168.4.1` |
| Канал | 6 |
| Макс. клиентов | 4 |

## Сборка и прошивка
```bash
idf.py set-target esp32c3
idf.py build
idf.py -p COM<N> flash monitor
```
Первая загрузка после смены sdkconfig.defaults: `del sdkconfig && idf.py fullclean`.

## Структура
- `main/main.c` — инициализация SPI/ILI9488/LVGL, запуск SoftAP
- `main/ui.c/.h` — экраны LVGL, `ui_set_bg_color()`, `ui_show_ap_banner()`
- `main/softap.c/.h` — Wi-Fi AP, DHCP, DNS-перехват, HTTP-сервер
- `assets/fonts/` — сгенерированные lv_font_conv шрифты (кириллица)
