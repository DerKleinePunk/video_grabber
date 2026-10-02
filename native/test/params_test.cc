#include "../src/params.h"

#include <string>

#include "check.h"

namespace {
bool Apply(const std::string& s, vg::CaptureConfig* c) {
  return vg::ApplyParams(reinterpret_cast<const uint8_t*>(s.data()), s.size(),
                         c);
}
}  // namespace

TEST_CASE(default_is_ntsc_composite0_video0) {
  const auto c = vg::DefaultConfig();
  EXPECT(!c.pal);
  EXPECT(c.width == 360);
  EXPECT(c.input == 0);
  EXPECT(c.device == "/dev/video0");
}

TEST_CASE(all_keys_are_applied) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("device=/dev/video2\ninput=4\nnorm=pal\n", &c));
  EXPECT(c.device == "/dev/video2");
  EXPECT(c.input == 4);
  EXPECT(c.pal);
  EXPECT(Apply("norm=ntsc", &c));
  EXPECT(!c.pal);
}

TEST_CASE(width_720_or_360) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("width=720", &c));
  EXPECT(c.width == 720);
  EXPECT(Apply("width=360", &c));
  EXPECT(c.width == 360);
  EXPECT(!Apply("width=640", &c));
  EXPECT(c.width == 360);
}

TEST_CASE(empty_params_keep_the_default) {
  auto c = vg::DefaultConfig();
  EXPECT(vg::ApplyParams(nullptr, 0, &c));
  EXPECT(Apply("", &c));
  EXPECT(!c.pal && c.input == 0);
}

TEST_CASE(unknown_keys_and_lines_without_equals_are_ignored) {
  auto c = vg::DefaultConfig();
  EXPECT(Apply("zoom=2\nnonsense\nnorm=pal", &c));
  EXPECT(c.pal);
}

TEST_CASE(invalid_values_report_failure_and_change_nothing) {
  auto c = vg::DefaultConfig();
  EXPECT(!Apply("norm=secam\ninput=x\ninput=1234\ndevice=video0", &c));
  EXPECT(!c.pal);
  EXPECT(c.input == 0);
  EXPECT(c.device == "/dev/video0");
  // valid values next to them still apply
  EXPECT(!Apply("norm=foo\ninput=2", &c));
  EXPECT(c.input == 2);
}

TEST_CASE(parse_uint_takes_one_to_three_digits_only) {
  uint32_t v = 7;
  EXPECT(vg::ParseUint("0", &v) && v == 0);
  EXPECT(vg::ParseUint("720", &v) && v == 720);
  EXPECT(vg::ParseUint("007", &v) && v == 7);
  v = 5;
  EXPECT(!vg::ParseUint("", &v));
  EXPECT(!vg::ParseUint("1234", &v));
  EXPECT(!vg::ParseUint("-1", &v));
  EXPECT(!vg::ParseUint("3x", &v));
  EXPECT(!vg::ParseUint(" 3", &v));
  EXPECT(v == 5);
}

int main() { return check::RunAll(); }
