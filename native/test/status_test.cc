#include "../src/status.h"

#include "check.h"

using vg::OpenResult;
using vg::Status;
using vg::WaitResult;

TEST_CASE(after_open_it_is_connecting) {
  vg::StatusTracker t;
  EXPECT(t.status() == Status::kConnecting);
  t.OnOpen(OpenResult::kOk, 100);
  EXPECT(t.status() == Status::kConnecting);
}

TEST_CASE(without_frames_no_signal_only_after_the_timeout) {
  vg::StatusTracker t(1000);
  t.OnOpen(OpenResult::kOk, 100);
  t.OnWait(WaitResult::kTimeout, 600);
  EXPECT(t.status() == Status::kConnecting);
  t.OnWait(WaitResult::kTimeout, 1100);
  EXPECT(t.status() == Status::kNoSignal);
}

TEST_CASE(frame_means_playing_and_signal_loss_is_detected) {
  vg::StatusTracker t(1000);
  t.OnOpen(OpenResult::kOk, 0);
  t.OnWait(WaitResult::kFrame, 50);
  EXPECT(t.status() == Status::kPlaying);
  t.OnWait(WaitResult::kTimeout, 900);
  EXPECT(t.status() == Status::kPlaying);
  t.OnWait(WaitResult::kTimeout, 1050);
  EXPECT(t.status() == Status::kNoSignal);
  t.OnWait(WaitResult::kFrame, 1100);
  EXPECT(t.status() == Status::kPlaying);
  EXPECT(t.frames() == 2);
}

TEST_CASE(missing_or_unplugged_device) {
  vg::StatusTracker t;
  t.OnOpen(OpenResult::kMissing, 0);
  EXPECT(t.status() == Status::kDeviceMissing);
  t.OnOpen(OpenResult::kOk, 10);
  t.OnWait(WaitResult::kFrame, 20);
  t.OnWait(WaitResult::kGone, 30);
  EXPECT(t.status() == Status::kDeviceMissing);
}

TEST_CASE(error_stays_until_the_next_open) {
  vg::StatusTracker t;
  t.OnOpen(OpenResult::kError, 0);
  EXPECT(t.status() == Status::kError);
  t.OnOpen(OpenResult::kOk, 5);
  EXPECT(t.status() == Status::kConnecting);
  t.OnWait(WaitResult::kError, 6);
  EXPECT(t.status() == Status::kError);
}

TEST_CASE(values_match_the_dart_side) {
  EXPECT(static_cast<int>(Status::kConnecting) == 0);
  EXPECT(static_cast<int>(Status::kPlaying) == 1);
  EXPECT(static_cast<int>(Status::kNoSignal) == 2);
  EXPECT(static_cast<int>(Status::kDeviceMissing) == 3);
  EXPECT(static_cast<int>(Status::kError) == 4);
}

int main() { return check::RunAll(); }
