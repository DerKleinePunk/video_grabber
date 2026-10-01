// Prüfprogramm für das Gerät: öffnet den Grabber, wartet auf Bilder und
// schreibt auf Wunsch ein Halbbild als NV16-Rohdatei.
//   vg_probe [/dev/video0] [eingang] [sekunden] [ausgabe.nv16]

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "uyvy.h"
#include "v4l2_capture.h"

int main(int argc, char** argv) {
  vg::CaptureConfig cfg;
  if (argc > 1) cfg.device = argv[1];
  if (argc > 2) cfg.input = static_cast<uint32_t>(std::atoi(argv[2]));
  if (const char* n = std::getenv("VG_NORM")) cfg.pal = std::string(n) == "pal";
  if (const char* w = std::getenv("VG_WIDTH")) cfg.width = static_cast<uint32_t>(std::atoi(w));
  const int seconds = argc > 3 ? std::atoi(argv[3]) : 3;
  const char* out_path = argc > 4 ? argv[4] : nullptr;

  vg::V4l2Capture cap(vg::RealSys());
  const auto open = cap.Open(cfg);
  if (open != vg::OpenResult::kOk) {
    std::printf("open: %s (%s)\n",
                open == vg::OpenResult::kMissing ? "fehlt" : "Fehler",
                cap.last_error().c_str());
    return 2;
  }
  std::printf("offen: %s %ux%u\n",
              cap.is_usb_camera() ? "USB-Kamera YUYV" : "Grabber UYVY",
              cap.width(), cap.height());

  const auto start = std::chrono::steady_clock::now();
  int frames = 0, timeouts = 0, incomplete = 0;
  size_t min_bytes = SIZE_MAX;
  uint64_t first_us = 0, last_us = 0;
  while (std::chrono::steady_clock::now() - start <
         std::chrono::seconds(seconds)) {
    vg::Frame f;
    switch (cap.Wait(500, &f)) {
      case vg::WaitResult::kFrame:
        if (frames == 0) first_us = f.timestamp_us;
        last_us = f.timestamp_us;
        if (!f.complete()) {
          ++incomplete;
          if (const char* d = std::getenv("VG_DUMP_INCOMPLETE")) {
            if (incomplete == 3) {
              if (FILE* o = std::fopen(d, "wb")) {
                std::fwrite(f.data, 1, f.stride * f.height, o);
                std::fclose(o);
                std::printf("unvollständig (%zu Byte) nach %s\n", f.bytes, d);
              }
            }
          }
        }
        if (f.bytes < min_bytes) min_bytes = f.bytes;
        if (frames == 0 && out_path != nullptr) {
          const vg::Field field = f.interlaced ? vg::Field::kTop : vg::Field::kBoth;
          const uint32_t h = vg::OutputHeight(f.height, field);
          std::vector<uint8_t> y(size_t{f.width} * h), uv(y.size());
          vg::UyvyToNv16(f.data, f.stride, f.width, f.height, field, y.data(),
                         f.width, uv.data(), f.width, f.packing);
          if (FILE* o = std::fopen(out_path, "wb")) {
            std::fwrite(y.data(), 1, y.size(), o);
            std::fwrite(uv.data(), 1, uv.size(), o);
            std::fclose(o);
            std::printf("Halbbild %ux%u NV16 nach %s\n", f.width, h, out_path);
          }
        }
        ++frames;
        cap.Release(f);
        break;
      case vg::WaitResult::kTimeout:
        ++timeouts;
        break;
      case vg::WaitResult::kGone:
        std::printf("Gerät weg\n");
        return 3;
      case vg::WaitResult::kError:
        std::printf("Fehler: %s\n", cap.last_error().c_str());
        return 4;
    }
  }
  const double fps =
      frames > 1 ? (frames - 1) * 1e6 / double(last_us - first_us) : 0.0;
  std::printf("%d Bilder (%.2f/s), davon %d unvollständig (kleinstes %zu von %zu Byte), %d x 500 ms ohne Bild\n",
              frames, fps, incomplete, min_bytes,
              size_t{cap.width()} * 2 * cap.height(), timeouts);
  return frames > 0 ? 0 : 1;
}
