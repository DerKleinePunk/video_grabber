// What the view needs to know about the source, derived from what the
// capture reports. A separate class so the transitions are testable without
// a device.

#pragma once

#include <cstdint>

#include "v4l2_capture.h"

namespace vg {

// Values as GrabberStatus in lib/src/grabber_source.dart (same order).
enum class Status : int32_t {
  kConnecting = 0,
  kPlaying = 1,
  kNoSignal = 2,
  kDeviceMissing = 3,
  kError = 4,
};

class StatusTracker {
 public:
  // If no frame arrives for this long, the signal counts as lost.
  explicit StatusTracker(uint64_t no_signal_after_ms = 1000)
      : no_signal_after_ms_(no_signal_after_ms) {}

  void OnOpen(OpenResult result, uint64_t now_ms);
  void OnWait(WaitResult result, uint64_t now_ms);

  [[nodiscard]] Status status() const { return status_; }
  [[nodiscard]] uint64_t frames() const { return frames_; }

 private:
  uint64_t no_signal_after_ms_;
  Status status_ = Status::kConnecting;
  uint64_t last_frame_ms_ = 0;
  uint64_t opened_ms_ = 0;
  uint64_t frames_ = 0;
};

}  // namespace vg
