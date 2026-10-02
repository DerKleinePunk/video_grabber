// Repacks grabber frames (UYVY 4:2:2, packed) into NV16 (4:2:2, Y plane plus
// interleaved CbCr plane). The Pi 4 display hardware (vc4) can scan out NV16
// directly on a plane, UYVY it cannot.

#pragma once

#include <cstddef>
#include <cstdint>

namespace vg {

// Which lines of a frame are taken. The STK1160 delivers a frame made of two
// interleaved fields. For a reversing camera one field is enough and has no
// combing artefacts.
enum class Field {
  kBoth,    // all lines (full frame, height lines)
  kTop,     // even lines 0, 2, 4, ... (height / 2 lines)
  kBottom,  // odd lines 1, 3, 5, ... (height / 2 lines)
};

// Output lines for @p height source lines.
uint32_t OutputHeight(uint32_t height, Field field);

// Packed 4:2:2: UYVY (STK1160) or YUYV (USB cameras, uvcvideo).
enum class Packing { kUyvy, kYuyv };

// Converts a packed frame to NV16 (UYVY by default). @p width must be even.
// Strides in bytes: the packed source needs width * 2 per line, Y width,
// CbCr width. Returns false if an argument does not fit; nothing is written
// then.
bool UyvyToNv16(const uint8_t* src,
                size_t src_stride,
                uint32_t width,
                uint32_t height,
                Field field,
                uint8_t* dst_y,
                size_t y_stride,
                uint8_t* dst_uv,
                size_t uv_stride,
                Packing packing = Packing::kUyvy);

}  // namespace vg
