// Capture from a V4L2 device: grabber (STK1160, UYVY, norm, input) or USB
// camera (YUYV, otherwise MJPEG/JPEG), mmap. System calls go through Sys so the
// tests can run without a device.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "uyvy.h"

namespace vg {

// Thin wrapper around the calls the capture needs. Return values and errno
// as with the real functions.
class Sys {
 public:
  virtual ~Sys() = default;
  virtual int Open(const char* path, int flags) = 0;
  virtual int Close(int fd) = 0;
  virtual int Ioctl(int fd, unsigned long request, void* arg) = 0;
  virtual void* Mmap(size_t length, int fd, long offset) = 0;
  virtual int Munmap(void* addr, size_t length) = 0;
  // poll() for POLLIN on @p fd; returns revents or -1.
  virtual int PollIn(int fd, int timeout_ms, short* revents) = 0;
  [[nodiscard]] virtual int Errno() const = 0;
};

std::unique_ptr<Sys> RealSys();

struct CaptureConfig {
  std::string device = "/dev/video0";
  uint32_t input = 0;  // Composite0 = yellow plug (check on the device)
  bool pal = true;     // otherwise NTSC
  // 720 or 360. At 720 USB is at its limit: on a Pi 4 ~2/3 of the frames
  // arrived incomplete, at 360 none.
  uint32_t width = 720;
  uint32_t buffers = 4;
  // USB camera: take MJPEG/JPEG even if the camera also offers YUYV. MJPEG
  // arrives much earlier (Jieli 1224:2a25: 57 instead of 154 ms to the
  // display, 30 instead of 25 frames/s); VG_FORMAT=yuyv goes back to YUYV.
  bool prefer_jpeg = true;
};

struct Frame {
  const uint8_t* data = nullptr;
  size_t bytes = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  size_t stride = 0;
  uint32_t index = 0;  // for Release()
  uint64_t timestamp_us = 0;

  Packing packing = Packing::kUyvy;
  // Compressed (MJPEG/JPEG, bytes vary): decode with JpegDecoder, packing
  // and stride do not apply.
  bool jpeg = false;
  bool interlaced = true;  // grabber: yes, USB camera: no (progressive)

  // When USB packets are lost the STK1160 delivers a shorter frame and the
  // rest is shifted. Do not show such frames. A broken JPEG frame is only
  // found by the decoder.
  [[nodiscard]] bool complete() const {
    return jpeg ? bytes > 0 : bytes >= stride * height;
  }
};

enum class WaitResult {
  kFrame,    // @p frame is filled, call Release() afterwards
  kTimeout,  // no frame in time: without a source the STK1160 sends nothing
  kGone,     // device unplugged (ENODEV/POLLERR), reopen
  kError,
};

enum class OpenResult { kOk, kMissing, kError };

class V4l2Capture {
 public:
  explicit V4l2Capture(std::unique_ptr<Sys> sys);
  ~V4l2Capture();
  V4l2Capture(const V4l2Capture&) = delete;
  V4l2Capture& operator=(const V4l2Capture&) = delete;

  // Opens the device, sets input, norm and format, allocates the buffers and
  // starts streaming.
  OpenResult Open(const CaptureConfig& config);
  WaitResult Wait(int timeout_ms, Frame* frame);
  void Release(const Frame& frame);
  void Close();

  [[nodiscard]] bool is_open() const { return fd_ >= 0; }
  [[nodiscard]] uint32_t width() const { return width_; }
  // Grabber (UYVY, norm, input, fields) or USB camera (YUYV or JPEG).
  [[nodiscard]] bool is_usb_camera() const {
    return jpeg_ || packing_ == Packing::kYuyv;
  }
  [[nodiscard]] bool is_jpeg() const { return jpeg_; }
  // For logs: "grabber UYVY", "USB camera YUYV" or "USB camera JPEG".
  [[nodiscard]] const char* source_name() const;
  [[nodiscard]] uint32_t height() const { return height_; }
  [[nodiscard]] const std::string& last_error() const { return last_error_; }

 private:
  OpenResult Fail(const char* what);

  std::unique_ptr<Sys> sys_;
  int fd_ = -1;
  bool streaming_ = false;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  size_t stride_ = 0;
  Packing packing_ = Packing::kUyvy;
  bool jpeg_ = false;
  uint32_t pixelformat_ = 0;
  bool interlaced_ = true;
  struct Mapping {
    void* addr;
    size_t length;
  };
  std::vector<Mapping> mappings_;
  std::string last_error_;
};

}  // namespace vg
