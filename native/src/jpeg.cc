#include "jpeg.h"

// jpeglib.h needs FILE and size_t declared before it.
#include <jerror.h>
#include <jpeglib.h>

#include <csetjmp>
#include <cstdio>
#include <vector>

namespace vg {

namespace {

// libjpeg reports fatal errors through error_exit, which must not return:
// it jumps back into Decode(). An exception would have to unwind through
// libjpeg, which is C and not built for that.
struct ErrorMgr {
  jpeg_error_mgr pub;
  std::jmp_buf jump;
  bool truncated;
  char message[JMSG_LENGTH_MAX];
};

void OnError(j_common_ptr cinfo) {
  auto* err = reinterpret_cast<ErrorMgr*>(cinfo->err);
  (*cinfo->err->format_message)(cinfo, err->message);
  // NOLINTNEXTLINE(cert-err52-cpp,modernize-avoid-setjmp-longjmp)
  std::longjmp(err->jump, 1);
}

// Warnings come with msg_level -1. A frame cut short is still decoded to the
// end (filled with grey); such frames are dropped. Other warnings and trace
// messages are ignored.
void OnMessage(j_common_ptr cinfo, int msg_level) {
  if (msg_level >= 0) {
    return;
  }
  const int code = cinfo->err->msg_code;
  if (code == JWRN_JPEG_EOF || code == JWRN_HIT_MARKER) {
    reinterpret_cast<ErrorMgr*>(cinfo->err)->truncated = true;
  }
}

enum class Result { kOk, kError, kTruncated, kWrongSize };

// Only trivially destructible objects here: libjpeg leaves through longjmp.
Result Decode(jpeg_decompress_struct* cinfo, ErrorMgr* err, const uint8_t* src,
              size_t size, uint32_t width, uint32_t height, uint8_t* row,
              uint8_t* dst_y, size_t y_stride, uint8_t* dst_uv,
              size_t uv_stride) {
  err->truncated = false;
  err->message[0] = '\0';
  // NOLINTNEXTLINE(cert-err52-cpp,modernize-avoid-setjmp-longjmp)
  if (setjmp(err->jump) != 0) {
    jpeg_abort_decompress(cinfo);
    return Result::kError;
  }
  jpeg_mem_src(cinfo, src, static_cast<unsigned long>(size));
  jpeg_read_header(cinfo, TRUE);
  if (cinfo->image_width != width || cinfo->image_height != height) {
    jpeg_abort_decompress(cinfo);
    return Result::kWrongSize;
  }
  // Y, Cb, Cr per pixel without colour conversion. Without fancy upsampling
  // the chroma samples are only repeated, so taking the even pixels gives the
  // 4:2:2 samples unchanged.
  cinfo->out_color_space = JCS_YCbCr;
  cinfo->do_fancy_upsampling = FALSE;
  cinfo->dct_method = JDCT_IFAST;
  jpeg_start_decompress(cinfo);
  while (cinfo->output_scanline < height) {
    const uint32_t line = cinfo->output_scanline;
    JSAMPROW rows[1] = {row};
    if (jpeg_read_scanlines(cinfo, rows, 1) != 1) {
      jpeg_abort_decompress(cinfo);
      return Result::kTruncated;
    }
    const uint8_t* p = row;
    uint8_t* y = dst_y + line * y_stride;
    uint8_t* uv = dst_uv + line * uv_stride;
    // Each 6 bytes Y0 Cb0 Cr0 Y1 Cb1 Cr1 → Y: Y0 Y1, CbCr: Cb0 Cr0.
    for (uint32_t x = 0; x < width; x += 2) {
      y[x] = p[0];
      uv[x] = p[1];
      uv[x + 1] = p[2];
      y[x + 1] = p[3];
      p += 6;
    }
  }
  jpeg_finish_decompress(cinfo);
  return err->truncated ? Result::kTruncated : Result::kOk;
}

}  // namespace

struct JpegDecoder::Impl {
  jpeg_decompress_struct cinfo{};
  ErrorMgr err{};
  std::vector<uint8_t> row;

  Impl() {
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = OnError;
    err.pub.emit_message = OnMessage;
    // Fails only without memory or with a mismatched library version.
    jpeg_create_decompress(&cinfo);
  }
  ~Impl() { jpeg_destroy_decompress(&cinfo); }
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
};

JpegDecoder::JpegDecoder() : impl_(std::make_unique<Impl>()) {}

JpegDecoder::~JpegDecoder() = default;

bool JpegDecoder::DecodeToNv16(const uint8_t* src, size_t size, uint32_t width,
                               uint32_t height, uint8_t* dst_y, size_t y_stride,
                               uint8_t* dst_uv, size_t uv_stride) {
  if (src == nullptr || size == 0 || dst_y == nullptr || dst_uv == nullptr ||
      width == 0 || (width % 2) != 0 || height == 0 || y_stride < width ||
      uv_stride < width) {
    last_error_ = "invalid arguments";
    return false;
  }
  impl_->row.resize(size_t{width} * 3);
  switch (Decode(&impl_->cinfo, &impl_->err, src, size, width, height,
                 impl_->row.data(), dst_y, y_stride, dst_uv, uv_stride)) {
    case Result::kOk:
      last_error_.clear();
      return true;
    case Result::kError:
      last_error_ = impl_->err.message;
      return false;
    case Result::kTruncated:
      last_error_ = "truncated frame";
      return false;
    case Result::kWrongSize:
      last_error_ = "unexpected image size";
      return false;
  }
  return false;
}

}  // namespace vg
