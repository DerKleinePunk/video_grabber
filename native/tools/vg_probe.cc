// Prüfprogramm für das Gerät: öffnet den Grabber, wartet auf Bilder und
// schreibt auf Wunsch ein Halbbild als NV16-Rohdatei.
//   vg_probe [/dev/video0] [eingang] [sekunden] [ausgabe.nv16]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "uyvy.h"
#include "v4l2_capture.h"

int main(int argc, char** argv) {
  vg::CaptureConfig cfg;
  if (argc > 1) cfg.device = argv[1];
  if (argc > 2) cfg.input = static_cast<uint32_t>(std::atoi(argv[2]));
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
  std::printf("offen: %ux%u, Eingang %u\n", cap.width(), cap.height(),
              cfg.input);

  const auto start = std::chrono::steady_clock::now();
  int frames = 0, timeouts = 0;
  uint64_t first_us = 0, last_us = 0;
  while (std::chrono::steady_clock::now() - start <
         std::chrono::seconds(seconds)) {
    vg::Frame f;
    switch (cap.Wait(500, &f)) {
      case vg::WaitResult::kFrame:
        if (frames == 0) first_us = f.timestamp_us;
        last_us = f.timestamp_us;
        if (frames == 0 && out_path != nullptr) {
          const uint32_t h = vg::OutputHeight(f.height, vg::Field::kTop);
          std::vector<uint8_t> y(size_t{f.width} * h), uv(y.size());
          vg::UyvyToNv16(f.data, f.stride, f.width, f.height, vg::Field::kTop,
                         y.data(), f.width, uv.data(), f.width);
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
  std::printf("%d Bilder (%.2f/s), %d x 500 ms ohne Bild\n", frames, fps,
              timeouts);
  return frames > 0 ? 0 : 1;
}
