// Device probe: opens the grabber, waits for frames and optionally writes
// one field as a raw NV16 file.
//   vg_probe [/dev/video0] [input] [seconds] [output.nv16]

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "params.h"
#include "uyvy.h"
#include "v4l2_capture.h"

namespace {

// Single-threaded tool: getenv() is safe here.
const char* Env(const char* name) {
  return std::getenv(name);  // NOLINT(concurrency-mt-unsafe)
}

int Usage() {
  std::fprintf(stderr,
               "usage: vg_probe [/dev/video0] [input] [seconds] "
               "[output.nv16]\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  vg::CaptureConfig cfg;
  uint32_t seconds = 3;
  if (argc > 1) {
    cfg.device = argv[1];
  }
  if (argc > 2 && !vg::ParseUint(argv[2], &cfg.input)) {
    return Usage();
  }
  if (argc > 3 && !vg::ParseUint(argv[3], &seconds)) {
    return Usage();
  }
  if (const char* n = Env("VG_NORM")) {
    cfg.pal = std::string(n) == "pal";
  }
  if (const char* w = Env("VG_WIDTH")) {
    if (!vg::ParseUint(w, &cfg.width)) {
      std::fprintf(stderr, "VG_WIDTH=%s ignored\n", w);
    }
  }
  const char* const dump_incomplete = Env("VG_DUMP_INCOMPLETE");
  const char* out_path = argc > 4 ? argv[4] : nullptr;

  vg::V4l2Capture cap(vg::RealSys());
  const auto open = cap.Open(cfg);
  if (open != vg::OpenResult::kOk) {
    std::printf("open: %s (%s)\n",
                open == vg::OpenResult::kMissing ? "missing" : "error",
                cap.last_error().c_str());
    return 2;
  }
  std::printf("open: %s %ux%u\n",
              cap.is_usb_camera() ? "USB camera YUYV" : "grabber UYVY",
              cap.width(), cap.height());

  const auto start = std::chrono::steady_clock::now();
  int frames = 0;
  int timeouts = 0;
  int incomplete = 0;
  size_t min_bytes = SIZE_MAX;
  uint64_t first_us = 0;
  uint64_t last_us = 0;
  while (std::chrono::steady_clock::now() - start <
         std::chrono::seconds(seconds)) {
    vg::Frame f;
    switch (cap.Wait(500, &f)) {
      case vg::WaitResult::kFrame:
        if (frames == 0) {
          first_us = f.timestamp_us;
        }
        last_us = f.timestamp_us;
        if (!f.complete()) {
          ++incomplete;
          if (const char* d = dump_incomplete) {
            if (incomplete == 3) {
              if (FILE* o = std::fopen(d, "wb")) {
                std::fwrite(f.data, 1, f.stride * f.height, o);
                std::fclose(o);
                std::printf("incomplete (%zu bytes) to %s\n", f.bytes, d);
              }
            }
          }
        }
        min_bytes = std::min(f.bytes, min_bytes);
        if (frames == 0 && out_path != nullptr) {
          const vg::Field field =
              f.interlaced ? vg::Field::kTop : vg::Field::kBoth;
          const uint32_t h = vg::OutputHeight(f.height, field);
          std::vector<uint8_t> y(size_t{f.width} * h);
          std::vector<uint8_t> uv(y.size());
          vg::UyvyToNv16(f.data, f.stride, f.width, f.height, field, y.data(),
                         f.width, uv.data(), f.width, f.packing);
          if (FILE* o = std::fopen(out_path, "wb")) {
            std::fwrite(y.data(), 1, y.size(), o);
            std::fwrite(uv.data(), 1, uv.size(), o);
            std::fclose(o);
            std::printf("field %ux%u NV16 to %s\n", f.width, h, out_path);
          }
        }
        ++frames;
        cap.Release(f);
        break;
      case vg::WaitResult::kTimeout:
        ++timeouts;
        break;
      case vg::WaitResult::kGone:
        std::printf("device gone\n");
        return 3;
      case vg::WaitResult::kError:
        std::printf("error: %s\n", cap.last_error().c_str());
        return 4;
    }
  }
  const double fps =
      frames > 1 ? (frames - 1) * 1e6 / static_cast<double>(last_us - first_us)
                 : 0.0;
  const size_t full_bytes = size_t{cap.width()} * 2 * cap.height();
  std::printf(
      "%d frames (%.2f/s), %d of them incomplete (smallest %zu of %zu bytes), "
      "%d x 500 ms without a frame\n",
      frames, fps, incomplete, min_bytes, full_bytes, timeouts);
  return frames > 0 ? 0 : 1;
}
