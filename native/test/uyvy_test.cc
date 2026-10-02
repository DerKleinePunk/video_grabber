#include "../src/uyvy.h"

#include <vector>

#include "check.h"

namespace {

// A UYVY frame in which every value tells where it came from:
// Y = 10 * row + column, U = 100 + row, V = 200 + row.
std::vector<uint8_t> MakeUyvy(uint32_t w, uint32_t h, size_t stride) {
  std::vector<uint8_t> img(stride * h, 0xEE);
  for (uint32_t r = 0; r < h; ++r) {
    for (uint32_t x = 0; x < w; x += 2) {
      uint8_t* p = img.data() + r * stride + x * 2;
      p[0] = static_cast<uint8_t>(100 + r);
      p[1] = static_cast<uint8_t>(10 * r + x);
      p[2] = static_cast<uint8_t>(200 + r);
      p[3] = static_cast<uint8_t>(10 * r + x + 1);
    }
  }
  return img;
}

struct Nv16 {
  Nv16(uint32_t h, size_t stride)
      : y(stride * h, 0), uv(stride * h, 0), stride(stride) {}
  std::vector<uint8_t> y, uv;
  size_t stride;
};

}  // namespace

TEST_CASE(full_frame_places_every_value_correctly) {
  const uint32_t w = 4, h = 3;
  auto src = MakeUyvy(w, h, w * 2);
  Nv16 out(h, w);
  EXPECT(vg::UyvyToNv16(src.data(), w * 2, w, h, vg::Field::kBoth,
                        out.y.data(), w, out.uv.data(), w));
  for (uint32_t r = 0; r < h; ++r) {
    for (uint32_t x = 0; x < w; ++x) {
      EXPECT(out.y[r * w + x] == 10 * r + x);
    }
    for (uint32_t x = 0; x < w; x += 2) {
      EXPECT(out.uv[r * w + x] == 100 + r);
      EXPECT(out.uv[r * w + x + 1] == 200 + r);
    }
  }
}

TEST_CASE(top_field_takes_the_even_lines) {
  const uint32_t w = 2, h = 6;
  auto src = MakeUyvy(w, h, w * 2);
  Nv16 out(3, w);
  EXPECT(vg::OutputHeight(h, vg::Field::kTop) == 3);
  EXPECT(vg::UyvyToNv16(src.data(), w * 2, w, h, vg::Field::kTop,
                        out.y.data(), w, out.uv.data(), w));
  for (uint32_t r = 0; r < 3; ++r) {
    EXPECT(out.y[r * w] == 10 * (2 * r));
    EXPECT(out.uv[r * w] == 100 + 2 * r);
  }
}

TEST_CASE(bottom_field_takes_the_odd_lines) {
  const uint32_t w = 2, h = 6;
  auto src = MakeUyvy(w, h, w * 2);
  Nv16 out(3, w);
  EXPECT(vg::UyvyToNv16(src.data(), w * 2, w, h, vg::Field::kBottom,
                        out.y.data(), w, out.uv.data(), w));
  for (uint32_t r = 0; r < 3; ++r) {
    EXPECT(out.y[r * w + 1] == 10 * (2 * r + 1) + 1);
    EXPECT(out.uv[r * w + 1] == 200 + 2 * r + 1);
  }
}

TEST_CASE(padded_strides_are_respected_and_not_overwritten) {
  const uint32_t w = 4, h = 2;
  const size_t src_stride = w * 2 + 8, dst_stride = w + 4;
  auto src = MakeUyvy(w, h, src_stride);
  std::vector<uint8_t> y(dst_stride * h, 0x55), uv(dst_stride * h, 0x55);
  EXPECT(vg::UyvyToNv16(src.data(), src_stride, w, h, vg::Field::kBoth,
                        y.data(), dst_stride, uv.data(), dst_stride));
  EXPECT(y[dst_stride + 3] == 13);
  EXPECT(uv[dst_stride + 1] == 201);
  for (uint32_t r = 0; r < h; ++r) {
    for (size_t x = w; x < dst_stride; ++x) {
      EXPECT(y[r * dst_stride + x] == 0x55);
      EXPECT(uv[r * dst_stride + x] == 0x55);
    }
  }
}

TEST_CASE(yuyv_is_repacked_correctly) {
  // Y0 U Y1 V per pair: Y = 10*row+column, U = 100+row, V = 200+row.
  const uint32_t w = 4, h = 2;
  std::vector<uint8_t> src(w * 2 * h);
  for (uint32_t r = 0; r < h; ++r) {
    for (uint32_t x = 0; x < w; x += 2) {
      uint8_t* p = src.data() + r * w * 2 + x * 2;
      p[0] = static_cast<uint8_t>(10 * r + x);
      p[1] = static_cast<uint8_t>(100 + r);
      p[2] = static_cast<uint8_t>(10 * r + x + 1);
      p[3] = static_cast<uint8_t>(200 + r);
    }
  }
  std::vector<uint8_t> y(w * h), uv(w * h);
  EXPECT(vg::UyvyToNv16(src.data(), w * 2, w, h, vg::Field::kBoth, y.data(),
                        w, uv.data(), w, vg::Packing::kYuyv));
  for (uint32_t r = 0; r < h; ++r) {
    for (uint32_t x = 0; x < w; ++x) EXPECT(y[r * w + x] == 10 * r + x);
    EXPECT(uv[r * w] == 100 + r);
    EXPECT(uv[r * w + 1] == 200 + r);
  }
}

TEST_CASE(pal_field_has_288_lines) {
  EXPECT(vg::OutputHeight(576, vg::Field::kTop) == 288);
  EXPECT(vg::OutputHeight(576, vg::Field::kBoth) == 576);
}

TEST_CASE(bad_arguments_are_rejected_without_writing) {
  std::vector<uint8_t> src(16, 1), y(8, 9), uv(8, 9);
  // odd width
  EXPECT(!vg::UyvyToNv16(src.data(), 8, 3, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  // source stride too small
  EXPECT(!vg::UyvyToNv16(src.data(), 6, 4, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  // destination stride too small
  EXPECT(!vg::UyvyToNv16(src.data(), 8, 4, 1, vg::Field::kBoth, y.data(), 3,
                         uv.data(), 4));
  EXPECT(!vg::UyvyToNv16(nullptr, 8, 4, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  for (auto b : y) EXPECT(b == 9);
}

int main() { return check::RunAll(); }
