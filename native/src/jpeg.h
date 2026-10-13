// Decodes JPEG frames (USB cameras: MJPEG from uvcvideo, JPEG from gspca)
// into NV16, the format the rest of the pipeline scans out. Uses libjpeg
// (libjpeg-turbo on the Pi). Any chroma subsampling is accepted; the output is
// always 4:2:2.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace vg {

class JpegDecoder {
 public:
  JpegDecoder();
  ~JpegDecoder();
  JpegDecoder(const JpegDecoder&) = delete;
  JpegDecoder& operator=(const JpegDecoder&) = delete;

  // Decodes @p size bytes at @p src. The image must be exactly
  // @p width x @p height, @p width even. Strides in bytes as in UyvyToNv16.
  // Returns false for broken or truncated data (USB cameras deliver those now
  // and then) or a different size; the output may be partly written then.
  bool DecodeToNv16(const uint8_t* src, size_t size, uint32_t width,
                    uint32_t height, uint8_t* dst_y, size_t y_stride,
                    uint8_t* dst_uv, size_t uv_stride);

  // Why the last DecodeToNv16() returned false.
  [[nodiscard]] const std::string& last_error() const { return last_error_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::string last_error_;
};

}  // namespace vg
