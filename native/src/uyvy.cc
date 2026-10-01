#include "uyvy.h"

namespace vg {

uint32_t OutputHeight(uint32_t height, Field field) {
  return field == Field::kBoth ? height : height / 2;
}

bool UyvyToNv16(const uint8_t* src,
                size_t src_stride,
                uint32_t width,
                uint32_t height,
                Field field,
                uint8_t* dst_y,
                size_t y_stride,
                uint8_t* dst_uv,
                size_t uv_stride,
                Packing packing) {
  if (src == nullptr || dst_y == nullptr || dst_uv == nullptr || width == 0 ||
      (width % 2) != 0 || src_stride < size_t{width} * 2 ||
      y_stride < width || uv_stride < width) {
    return false;
  }
  const uint32_t out_height = OutputHeight(height, field);
  const uint32_t first = field == Field::kBottom ? 1 : 0;
  const uint32_t step = field == Field::kBoth ? 1 : 2;

  for (uint32_t row = 0; row < out_height; ++row) {
    const uint8_t* s = src + (first + row * step) * src_stride;
    uint8_t* y = dst_y + row * y_stride;
    uint8_t* uv = dst_uv + row * uv_stride;
    if (packing == Packing::kUyvy) {
      // Je 4 Byte U0 Y0 V0 Y1 → Y: Y0 Y1, CbCr: U0 V0.
      for (uint32_t x = 0; x < width; x += 2) {
        uv[x] = s[0];
        y[x] = s[1];
        uv[x + 1] = s[2];
        y[x + 1] = s[3];
        s += 4;
      }
    } else {
      // Je 4 Byte Y0 U0 Y1 V0.
      for (uint32_t x = 0; x < width; x += 2) {
        y[x] = s[0];
        uv[x] = s[1];
        y[x + 1] = s[2];
        uv[x + 1] = s[3];
        s += 4;
      }
    }
  }
  return true;
}

}  // namespace vg
