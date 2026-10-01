#include "../src/uyvy.h"

#include <vector>

#include "check.h"

namespace {

// Ein UYVY-Bild, in dem jeder Wert seine Herkunft verrät:
// Y = 10 * Zeile + Spalte, U = 100 + Zeile, V = 200 + Zeile.
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

TEST_CASE(vollbild_sortiert_jeden_wert_richtig) {
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

TEST_CASE(oberes_halbbild_nimmt_die_geraden_zeilen) {
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

TEST_CASE(unteres_halbbild_nimmt_die_ungeraden_zeilen) {
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

TEST_CASE(strides_mit_rand_werden_beachtet_und_nicht_ueberschrieben) {
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

TEST_CASE(yuyv_wird_richtig_umsortiert) {
  // Y0 U Y1 V je Paar: Y = 10*Zeile+Spalte, U = 100+Zeile, V = 200+Zeile.
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

TEST_CASE(pal_halbbild_hat_288_zeilen) {
  EXPECT(vg::OutputHeight(576, vg::Field::kTop) == 288);
  EXPECT(vg::OutputHeight(576, vg::Field::kBoth) == 576);
}

TEST_CASE(falsche_argumente_werden_abgelehnt_ohne_zu_schreiben) {
  std::vector<uint8_t> src(16, 1), y(8, 9), uv(8, 9);
  // ungerade Breite
  EXPECT(!vg::UyvyToNv16(src.data(), 8, 3, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  // Quell-Stride zu klein
  EXPECT(!vg::UyvyToNv16(src.data(), 6, 4, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  // Ziel-Stride zu klein
  EXPECT(!vg::UyvyToNv16(src.data(), 8, 4, 1, vg::Field::kBoth, y.data(), 3,
                         uv.data(), 4));
  EXPECT(!vg::UyvyToNv16(nullptr, 8, 4, 1, vg::Field::kBoth, y.data(), 4,
                         uv.data(), 4));
  for (auto b : y) EXPECT(b == 9);
}

int main() { return check::RunAll(); }
