#include "status.h"

namespace vg {

void StatusTracker::OnOpen(OpenResult result, uint64_t now_ms) {
  switch (result) {
    case OpenResult::kOk:
      status_ = Status::kConnecting;
      opened_ms_ = now_ms;
      last_frame_ms_ = 0;
      break;
    case OpenResult::kMissing:
      status_ = Status::kDeviceMissing;
      break;
    case OpenResult::kError:
      status_ = Status::kError;
      break;
  }
}

void StatusTracker::OnWait(WaitResult result, uint64_t now_ms) {
  switch (result) {
    case WaitResult::kFrame:
      status_ = Status::kPlaying;
      last_frame_ms_ = now_ms;
      ++frames_;
      break;
    case WaitResult::kTimeout: {
      const uint64_t since = last_frame_ms_ != 0 ? last_frame_ms_ : opened_ms_;
      if (now_ms - since >= no_signal_after_ms_) {
        status_ = Status::kNoSignal;
      }
      break;
    }
    case WaitResult::kGone:
      status_ = Status::kDeviceMissing;
      break;
    case WaitResult::kError:
      status_ = Status::kError;
      break;
  }
}

}  // namespace vg
