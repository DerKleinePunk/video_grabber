#include "../src/status.h"

#include "check.h"

using vg::OpenResult;
using vg::Status;
using vg::WaitResult;

TEST_CASE(nach_dem_oeffnen_verbindet_es) {
  vg::StatusTracker t;
  EXPECT(t.status() == Status::kConnecting);
  t.OnOpen(OpenResult::kOk, 100);
  EXPECT(t.status() == Status::kConnecting);
}

TEST_CASE(ohne_bild_kommt_kein_signal_erst_nach_der_frist) {
  vg::StatusTracker t(1000);
  t.OnOpen(OpenResult::kOk, 100);
  t.OnWait(WaitResult::kTimeout, 600);
  EXPECT(t.status() == Status::kConnecting);
  t.OnWait(WaitResult::kTimeout, 1100);
  EXPECT(t.status() == Status::kNoSignal);
}

TEST_CASE(bild_heisst_abspielen_und_signalverlust_wird_erkannt) {
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

TEST_CASE(fehlendes_oder_abgezogenes_geraet) {
  vg::StatusTracker t;
  t.OnOpen(OpenResult::kMissing, 0);
  EXPECT(t.status() == Status::kDeviceMissing);
  t.OnOpen(OpenResult::kOk, 10);
  t.OnWait(WaitResult::kFrame, 20);
  t.OnWait(WaitResult::kGone, 30);
  EXPECT(t.status() == Status::kDeviceMissing);
}

TEST_CASE(fehler_bleibt_fehler_bis_zum_naechsten_oeffnen) {
  vg::StatusTracker t;
  t.OnOpen(OpenResult::kError, 0);
  EXPECT(t.status() == Status::kError);
  t.OnOpen(OpenResult::kOk, 5);
  EXPECT(t.status() == Status::kConnecting);
  t.OnWait(WaitResult::kError, 6);
  EXPECT(t.status() == Status::kError);
}

TEST_CASE(werte_passen_zur_dart_seite) {
  EXPECT(static_cast<int>(Status::kConnecting) == 0);
  EXPECT(static_cast<int>(Status::kPlaying) == 1);
  EXPECT(static_cast<int>(Status::kNoSignal) == 2);
  EXPECT(static_cast<int>(Status::kDeviceMissing) == 3);
  EXPECT(static_cast<int>(Status::kError) == 4);
}

int main() { return check::RunAll(); }
