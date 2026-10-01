// Was die Anzeige über die Quelle wissen muss, abgeleitet aus dem, was die
// Aufnahme meldet. Eigene Klasse, damit die Übergänge ohne Gerät testbar sind.

#pragma once

#include <cstdint>

#include "v4l2_capture.h"

namespace vg {

// Werte wie GrabberStatus in lib/src/grabber_source.dart (gleiche Reihenfolge).
enum class Status : int32_t {
  kConnecting = 0,
  kPlaying = 1,
  kNoSignal = 2,
  kDeviceMissing = 3,
  kError = 4,
};

class StatusTracker {
 public:
  // Kommt so lange kein Bild, gilt das Signal als weg.
  explicit StatusTracker(uint64_t no_signal_after_ms = 1000)
      : no_signal_after_ms_(no_signal_after_ms) {}

  void OnOpen(OpenResult result, uint64_t now_ms);
  void OnWait(WaitResult result, uint64_t now_ms);

  Status status() const { return status_; }
  uint64_t frames() const { return frames_; }

 private:
  uint64_t no_signal_after_ms_;
  Status status_ = Status::kConnecting;
  uint64_t last_frame_ms_ = 0;
  uint64_t opened_ms_ = 0;
  uint64_t frames_ = 0;
};

}  // namespace vg
