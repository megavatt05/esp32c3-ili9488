#!/bin/bash
# Helper script for converting TTF → LVGL font with Cyrillic support
# Usage: ./scripts/convert_font.sh Inter-Regular.ttf 20

set -e

if [ $# -lt 2 ]; then
  echo "Usage: $0 <font.ttf> <size> [output_name]"
  echo "Example: $0 fonts/Inter-Regular.ttf 20 inter_20"
  exit 1
fi

FONT_FILE="$1"
SIZE="$2"
NAME="${3:-font_$(basename "$FONT_FILE" .ttf)_${SIZE}}"
OUTPUT="assets/fonts/${NAME}.c"

echo "Converting $FONT_FILE → $OUTPUT (size=$SIZE, bpp=4, Latin+Cyrillic)"

lv_font_conv \
  --font "$FONT_FILE" \
  --size "$SIZE" \
  --bpp 4 \
  --format lvgl \
  --no-compress \
  -r 0x20-0x7F \
  -r 0x400-0x4FF \
  --symbols "°С±µ•—–“”«»" \
  -o "$OUTPUT"

echo "Done: $OUTPUT"
echo "Add to main/CMakeLists.txt SRCS and use:"
echo "  LV_FONT_DECLARE($NAME);"
echo "  lv_obj_set_style_text_font(label, &$NAME, 0);"
