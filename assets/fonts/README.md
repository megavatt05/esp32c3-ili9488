# Custom Cyrillic Fonts

These fonts were generated with `lv_font_conv` (bpp=4).

| File                   | Font       | Size | Approx size |
|------------------------|------------|------|-------------|
| font_inter_16.c        | Inter      | 16px | 161 KB      |
| font_roboto_20.c       | Roboto     | 20px | 241 KB      |
| font_montserrat_24.c   | Montserrat | 24px | 277 KB      |
| font_notosans_28.c     | Noto Sans  | 28px | 376 KB      |

## How to get the real files

The generated `.c` files are large. Copy them into this folder from the place where they were created, then uncomment the corresponding lines in `main/CMakeLists.txt`.

Generation commands used:

```bash
lv_font_conv --font Inter-Regular.ttf --size 16 --bpp 4 --format lvgl --no-compress \
  -r 0x20-0x7F -r 0x400-0x4FF --symbols "°С±•—–“”«»" -o font_inter_16.c

lv_font_conv --font Roboto-Regular.ttf --size 20 --bpp 4 --format lvgl --no-compress \
  -r 0x20-0x7F -r 0x400-0x4FF --symbols "°С±•—–“”«»" -o font_roboto_20.c

lv_font_conv --font Montserrat-Regular.ttf --size 24 --bpp 4 --format lvgl --no-compress \
  -r 0x20-0x7F -r 0x400-0x4FF --symbols "°С±•—–“”«»" -o font_montserrat_24.c

lv_font_conv --font NotoSans-Regular.ttf --size 28 --bpp 4 --format lvgl --no-compress \
  -r 0x20-0x7F -r 0x400-0x4FF --symbols "°С±•—–“”«»" -o font_notosans_28.c
```
