#!/bin/bash
# Helper for image conversion (uses online tool recommendation or lv_img_conv if available)
# Usage: ./scripts/convert_image.sh photo.png

set -e

if [ $# -lt 1 ]; then
  echo "Usage: $0 <image.png|jpg> [output_name]"
  echo ""
  echo "Recommended: use the official online converter:"
  echo "  https://lvgl.io/tools/imageconverter"
  echo "  Color format: RGB565"
  echo "  Output: C array"
  echo "  Then place the .c file into assets/images/"
  exit 1
fi

IMG="$1"
NAME="${2:-img_$(basename "$IMG" | sed 's/\..*//')}"
OUTPUT="assets/images/${NAME}.c"

echo "Please convert $IMG using:"
echo "  https://lvgl.io/tools/imageconverter"
echo "Settings:"
echo "  - Color format: RGB565 (or RGB565A8 if transparency needed)"
echo "  - Output format: C array"
echo "  - Name: $NAME"
echo ""
echo "Save the result as: $OUTPUT"
echo ""
echo "Then add to CMakeLists.txt and use:"
echo "  LV_IMAGE_DECLARE($NAME);"
echo "  lv_image_set_src(img, &$NAME);"
