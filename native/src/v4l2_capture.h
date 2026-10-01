// Aufnahme von einem V4L2-Grabber (STK1160): PAL, ein Eingang, UYVY, mmap.
// Die Systemaufrufe laufen über Sys, damit die Tests ohne Gerät auskommen.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "uyvy.h"

namespace vg {

// Dünne Hülle um die Aufrufe, die die Aufnahme braucht. Rückgabewerte und
// errno wie bei den echten Funktionen.
class Sys {
 public:
  virtual ~Sys() = default;
  virtual int Open(const char* path, int flags) = 0;
  virtual int Close(int fd) = 0;
  virtual int Ioctl(int fd, unsigned long request, void* arg) = 0;
  virtual void* Mmap(size_t length, int fd, long offset) = 0;
  virtual int Munmap(void* addr, size_t length) = 0;
  // poll() auf POLLIN für @p fd; liefert revents oder -1.
  virtual int PollIn(int fd, int timeout_ms, short* revents) = 0;
  virtual int Errno() const = 0;
};

std::unique_ptr<Sys> RealSys();

struct CaptureConfig {
  std::string device = "/dev/video0";
  uint32_t input = 0;   // Composite0 = gelber Stecker (am Gerät prüfen)
  bool pal = true;      // sonst NTSC
  // 720 oder 360. Bei 720 ist der USB am Anschlag: auf jeep-pi kamen ~2/3
  // der Bilder unvollständig an, bei 360 keines.
  uint32_t width = 720;
  uint32_t buffers = 4;
};

struct Frame {
  const uint8_t* data = nullptr;
  size_t bytes = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  size_t stride = 0;
  uint32_t index = 0;   // für Release()
  uint64_t timestamp_us = 0;

  Packing packing = Packing::kUyvy;
  bool interlaced = true;  // Grabber: ja, USB-Kamera: nein (Vollbild)

  // Fehlen am USB Pakete, liefert der STK1160 ein kürzeres Bild; der Rest ist
  // dann verschoben. Solche Bilder nicht zeigen.
  bool complete() const { return bytes >= stride * height; }
};

enum class WaitResult {
  kFrame,    // @p frame ist gefüllt, danach Release() aufrufen
  kTimeout,  // in der Frist kein Bild: ohne Quelle liefert der STK1160 nichts
  kGone,     // Gerät abgezogen (ENODEV/POLLERR), neu öffnen
  kError,
};

enum class OpenResult { kOk, kMissing, kError };

class V4l2Capture {
 public:
  explicit V4l2Capture(std::unique_ptr<Sys> sys);
  ~V4l2Capture();
  V4l2Capture(const V4l2Capture&) = delete;
  V4l2Capture& operator=(const V4l2Capture&) = delete;

  // Öffnet, stellt Eingang, Norm und Format ein, legt die Puffer an und
  // startet den Strom.
  OpenResult Open(const CaptureConfig& config);
  WaitResult Wait(int timeout_ms, Frame* frame);
  void Release(const Frame& frame);
  void Close();

  bool is_open() const { return fd_ >= 0; }
  uint32_t width() const { return width_; }
  // Grabber (UYVY, Norm, Eingang, Halbbilder) oder USB-Kamera (YUYV).
  bool is_usb_camera() const { return packing_ == Packing::kYuyv; }
  uint32_t height() const { return height_; }
  const std::string& last_error() const { return last_error_; }

 private:
  OpenResult Fail(const char* what);

  std::unique_ptr<Sys> sys_;
  int fd_ = -1;
  bool streaming_ = false;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  size_t stride_ = 0;
  Packing packing_ = Packing::kUyvy;
  bool interlaced_ = true;
  struct Mapping {
    void* addr;
    size_t length;
  };
  std::vector<Mapping> mappings_;
  std::string last_error_;
};

}  // namespace vg
