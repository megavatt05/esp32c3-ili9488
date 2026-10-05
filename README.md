# ESP32-C3 + ILI9488 + LVGL 9

Ветка с полноценным примером **LVGL 9**.

## Особенности

- Красивый тёмный UI со скруглёнными карточками
- Поддержка кириллицы (шрифты Montserrat)
- Плавная работа на ESP32-C3 (частичный буфер + double buffering)
- SPI 40 МГц
- Без Wi-Fi/BT для экономии RAM

## Распиновка (та же)

| Сигнал | GPIO |
|--------|------|
| SCK    | 2    |
| MOSI   | 4    |
| CS     | 5    |
| DC     | 1    |
| RST    | 0    |
| BL     | 3.3V |

## Сборка

```bash
git checkout lvgl9
idf.py set-target esp32c3
idf.py build
idf.py -p PORT flash monitor
```

## Как добавить свой красивый шрифт с кириллицей

1. Возьми TTF (например Inter, Roboto, Montserrat)
2. Используй онлайн-конвертер: https://lvgl.io/tools/fontconverter
   - Size: 18–24
 * Bpp: 4 (рекомендуется)
 * Range: `0x20-0x7F,0x400-0x4FF` (латиница + кириллица)
3. Скачай `.c` файл
4. Добавь в `main/` и объяви `LV_FONT_DECLARE(my_font);`
5. Используй: `lv_obj_set_style_text_font(label, &my_font, 0);`

## Производительность

На ESP32-C3 при 40 МГц SPI + partial buffer 30 строк интерфейс остаётся плавным.
Если появятся артефакты — снизь `pclk_hz` до 30 МГц.
