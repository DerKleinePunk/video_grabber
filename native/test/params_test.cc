#include "../src/params.h"

#include <string>

#include "check.h"

namespace {
bool Apply(const std::string& s, vg::CaptureConfig* c) {
  return vg::ApplyParams(reinterpret_cast<const uint8_t*>(s.data()), s.size(),
                         c);
}
}  // namespace

TEST_CASE(vorgabe_ist_ntsc_composite0_video0) {
  const auto c = vg::DefaultConfig();
  EXPECT(!c.pal);
  EXPECT(c.width == 360);
  EXPECT(c.input == 0);
  EXPECT(c.device == "/dev/video0");
}

TEST_CASE(alle_schluessel_werden_uebernommen) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("device=/dev/video2\ninput=4\nnorm=pal\n", &c));
  EXPECT(c.device == "/dev/video2");
  EXPECT(c.input == 4);
  EXPECT(c.pal);
  EXPECT(Apply("norm=ntsc", &c));
  EXPECT(!c.pal);
}

TEST_CASE(breite_720_oder_360) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("width=720", &c));
  EXPECT(c.width == 720);
  EXPECT(Apply("width=360", &c));
  EXPECT(c.width == 360);
  EXPECT(!Apply("width=640", &c));
  EXPECT(c.width == 360);
}

TEST_CASE(leere_parameter_lassen_die_vorgabe) {
  auto c = vg::DefaultConfig();
  EXPECT(vg::ApplyParams(nullptr, 0, &c));
  EXPECT(Apply("", &c));
  EXPECT(!c.pal && c.input == 0);
}

TEST_CASE(unbekannte_schluessel_und_zeilen_ohne_gleich_werden_ignoriert) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("zoom=2\nunsinn\nnorm=pal", &c));
  EXPECT(c.pal);
}

TEST_CASE(ungueltige_werte_melden_fehler_und_aendern_nichts) {
  auto c = vg::DefaultConfig();
  EXPECT(!Apply("norm=secam\ninput=x\ninput=1234\ndevice=video0", &c));
  EXPECT(!c.pal);
  EXPECT(c.input == 0);
  EXPECT(c.device == "/dev/video0");
  // gültige Werte daneben gelten trotzdem
  EXPECT(!Apply("norm=foo\ninput=2", &c));
  EXPECT(c.input == 2);
}

int main() { return check::RunAll(); }
