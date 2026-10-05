# ESP32-C3 + ILI9488 + LVGL 9

Красивый пример с поддержкой кириллицы, скруглённых шрифтов и структурой для ассетов.

## Структура проекта

```
├── assets/
│   ├── fonts/          ← сюда класть сгенерированные .c шрифты
│   └── images/         ← сюда класть сгенерированные .c картинки
├── scripts/
│   ├── convert_font.sh ← помощник для lv_font_conv (кириллица)
│   └── convert_image.sh
├── main/
│   ├── main.c
│   ├── ui.c / ui.h
│   └── CMakeLists.txt
└── ...
```

## Быстрый старт

```bash
git checkout lvgl9
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Как добавить красивый шрифт с кириллицей

### Вариант 1 — скрипт (рекомендуется)

```bash
# Нужен Node.js + lv_font_conv
npm install -g lv_font_conv

# Конвертация
./scripts/convert_font.sh /path/to/Inter-Regular.ttf 20 inter_20
```

Скрипт автоматически добавляет диапазоны:
- `0x20-0x7F` (латиница)
- `0x400-0x4FF` (кириллица)
- bpp = 4 (красивое сглаживание)

### Вариант 2 — онлайн

https://lvgl.io/tools/fontconverter

- Size: 20–24
- Bpp: **4**
- Range: `0x20-0x7F,0x400-0x4FF`

### Использование

```c
LV_FONT_DECLARE(inter_20);
lv_obj_set_style_text_font(label, &inter_20, 0);
```

Не забудь добавить `.c` файл в `main/CMakeLists.txt` → `SRCS`.

## Как добавить картинку

1. Конвертируй на https://lvgl.io/tools/imageconverter
   - Color format: **RGB565**
   - Output: C array
2. Положи файл в `assets/images/`
3. Добавь в `SRCS` CMakeLists.txt
4. В коде:

```c
LV_IMAGE_DECLARE(my_bg);
lv_obj_t * img = lv_image_create(parent);
lv_image_set_src(img, &my_bg);
```

## Распиновка

| Сигнал | GPIO |
|--------|------|
| SCK    | 2    |
| MOSI   | 4    |
| CS     | 5    |
| DC     | 1    |
| RST    | 0    |
| BL     | 3.3V |
