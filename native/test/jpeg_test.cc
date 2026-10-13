#include "../src/jpeg.h"

// jpeglib.h needs FILE and size_t declared before it.
#include <jpeglib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "check.h"

namespace {

// Test picture (up to 64x64): smooth so that quality 100 comes back almost
// unchanged.
uint8_t Luma(uint32_t x, uint32_t y) { return 16 + x * 2 + y; }
uint8_t Cb(uint32_t x, uint32_t y) { return 64 + (x / 16) * 8 + (y / 16) * 4; }
uint8_t Cr(uint32_t x, uint32_t y) { return 192 - (x / 16) * 4 - (y / 16) * 8; }

// Encodes the test picture as baseline JPEG with the given chroma sampling
// (h, v = 2, 1 → 4:2:2; 2, 2 → 4:2:0; 1, 1 → 4:4:4).
std::vector<uint8_t> Encode(uint32_t width, uint32_t height, int h, int v) {
  jpeg_compress_struct c{};
  jpeg_error_mgr err{};
  c.err = jpeg_std_error(&err);
  jpeg_create_compress(&c);
  unsigned char* out = nullptr;
  unsigned long out_size = 0;
  jpeg_mem_dest(&c, &out, &out_size);
  c.image_width = width;
  c.image_height = height;
  c.input_components = 3;
  c.in_color_space = JCS_YCbCr;
  jpeg_set_defaults(&c);
  jpeg_set_quality(&c, 100, TRUE);
  c.comp_info[0].h_samp_factor = h;
  c.comp_info[0].v_samp_factor = v;
  jpeg_start_compress(&c, TRUE);
  std::vector<uint8_t> row(size_t{width} * 3);
  while (c.next_scanline < height) {
    const uint32_t y = c.next_scanline;
    for (uint32_t x = 0; x < width; ++x) {
      row[x * 3] = Luma(x, y);
      row[x * 3 + 1] = Cb(x, y);
      row[x * 3 + 2] = Cr(x, y);
    }
    JSAMPROW rows[1] = {row.data()};
    jpeg_write_scanlines(&c, rows, 1);
  }
  jpeg_finish_compress(&c);
  std::vector<uint8_t> data(out, out + out_size);
  jpeg_destroy_compress(&c);
  std::free(out);  // NOLINT(cppcoreguidelines-no-malloc): libjpeg's malloc
  return data;
}

// MJPEG from many USB cameras leaves out the Huffman tables (DHT, FF C4) and
// relies on the standard tables.
std::vector<uint8_t> WithoutHuffmanTables(const std::vector<uint8_t>& jpeg) {
  std::vector<uint8_t> out(jpeg.begin(), jpeg.begin() + 2);  // SOI
  size_t pos = 2;
  while (pos + 4 <= jpeg.size() && jpeg[pos] == 0xFF) {
    const uint8_t marker = jpeg[pos + 1];
    const size_t length = (size_t{jpeg[pos + 2]} << 8) | jpeg[pos + 3];
    if (marker == 0xDA) {  // SOS: the rest is scan data
      out.insert(out.end(), jpeg.begin() + static_cast<long>(pos), jpeg.end());
      return out;
    }
    if (marker != 0xC4) {
      out.insert(out.end(), jpeg.begin() + static_cast<long>(pos),
                 jpeg.begin() + static_cast<long>(pos + 2 + length));
    }
    pos += 2 + length;
  }
  return out;
}

int Diff(uint8_t a, uint8_t b) { return a > b ? a - b : b - a; }

struct Nv16 {
  uint32_t width, height;
  size_t stride;
  std::vector<uint8_t> y, uv;
  Nv16(uint32_t w, uint32_t h, size_t s)
      : width(w), height(h), stride(s), y(s * h, 0xAA), uv(s * h, 0xAA) {}
  bool Decode(vg::JpegDecoder* d, const std::vector<uint8_t>& jpeg) {
    return d->DecodeToNv16(jpeg.data(), jpeg.size(), width, height, y.data(),
                           stride, uv.data(), stride);
  }
  // Largest deviation from the test picture. Chroma is compared with the
  // average over the 2x2 (4:2:0) or 2x1 (4:2:2) area it stands for.
  [[nodiscard]] int MaxError() const {
    int worst = 0;
    for (uint32_t row = 0; row < height; ++row) {
      for (uint32_t x = 0; x < width; ++x) {
        worst = std::max(worst, Diff(y[row * stride + x], Luma(x, row)));
      }
      for (uint32_t x = 0; x < width; x += 2) {
        worst = std::max(worst, Diff(uv[row * stride + x], Cb(x, row)));
        worst = std::max(worst, Diff(uv[row * stride + x + 1], Cr(x, row)));
      }
    }
    return worst;
  }
};

}  // namespace

TEST_CASE(decodes_422) {
  vg::JpegDecoder d;
  Nv16 out(64, 32, 64);
  EXPECT(out.Decode(&d, Encode(64, 32, 2, 1)));
  EXPECT(out.MaxError() <= 8);
  EXPECT(d.last_error().empty());
}

TEST_CASE(decodes_420_and_444) {
  vg::JpegDecoder d;
  Nv16 a(64, 32, 64);
  EXPECT(a.Decode(&d, Encode(64, 32, 2, 2)));
  EXPECT(a.MaxError() <= 8);
  Nv16 b(64, 32, 64);
  EXPECT(b.Decode(&d, Encode(64, 32, 1, 1)));
  EXPECT(b.MaxError() <= 8);
}

TEST_CASE(stride_padding_stays_untouched) {
  vg::JpegDecoder d;
  Nv16 out(48, 16, 64);
  EXPECT(out.Decode(&d, Encode(48, 16, 2, 1)));
  EXPECT(out.MaxError() <= 8);
  bool untouched = true;
  for (uint32_t row = 0; row < 16; ++row) {
    for (uint32_t x = 48; x < 64; ++x) {
      untouched = untouched && out.y[row * 64 + x] == 0xAA &&
                  out.uv[row * 64 + x] == 0xAA;
    }
  }
  EXPECT(untouched);
}

TEST_CASE(mjpeg_without_huffman_tables) {
  // Encode() uses the standard tables (no optimize_coding), so leaving them
  // out changes nothing for a decoder that knows them.
  const auto full = Encode(64, 32, 2, 1);
  const auto bare = WithoutHuffmanTables(full);
  EXPECT(bare.size() < full.size());
  vg::JpegDecoder d;
  Nv16 out(64, 32, 64);
  EXPECT(out.Decode(&d, bare));
  EXPECT(out.MaxError() <= 8);
}

TEST_CASE(truncated_frame_is_rejected) {
  auto jpeg = Encode(64, 64, 2, 1);
  jpeg.resize(jpeg.size() / 2);
  vg::JpegDecoder d;
  Nv16 out(64, 64, 64);
  EXPECT(!out.Decode(&d, jpeg));
  EXPECT(d.last_error() == "truncated frame");
}

TEST_CASE(garbage_is_rejected_and_the_decoder_stays_usable) {
  vg::JpegDecoder d;
  Nv16 out(64, 32, 64);
  const std::vector<uint8_t> garbage(500, 0x5A);
  EXPECT(!out.Decode(&d, garbage));
  EXPECT(!d.last_error().empty());
  auto broken = Encode(64, 32, 2, 1);
  broken[1] = 0x00;  // no SOI marker: not a JPEG
  EXPECT(!out.Decode(&d, broken));
  EXPECT(out.Decode(&d, Encode(64, 32, 2, 1)));
  EXPECT(out.MaxError() <= 8);
}

TEST_CASE(other_size_is_rejected) {
  vg::JpegDecoder d;
  Nv16 out(64, 32, 64);
  EXPECT(!out.Decode(&d, Encode(32, 32, 2, 1)));
  EXPECT(d.last_error() == "unexpected image size");
}

TEST_CASE(invalid_arguments) {
  vg::JpegDecoder d;
  const auto jpeg = Encode(64, 32, 2, 1);
  std::vector<uint8_t> y(64 * 32);
  std::vector<uint8_t> uv(64 * 32);
  EXPECT(!d.DecodeToNv16(nullptr, 10, 64, 32, y.data(), 64, uv.data(), 64));
  EXPECT(!d.DecodeToNv16(jpeg.data(), 0, 64, 32, y.data(), 64, uv.data(), 64));
  EXPECT(!d.DecodeToNv16(jpeg.data(), jpeg.size(), 63, 32, y.data(), 64,
                         uv.data(), 64));
  EXPECT(!d.DecodeToNv16(jpeg.data(), jpeg.size(), 64, 32, y.data(), 32,
                         uv.data(), 64));
  EXPECT(d.last_error() == "invalid arguments");
}

int main() { return check::RunAll(); }
