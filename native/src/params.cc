#include "params.h"

#include <cstdlib>

namespace vg {

CaptureConfig DefaultConfig() {
  CaptureConfig c;
  c.pal = false;
  c.width = 360;
  return c;
}

namespace {

bool ParseUint(const std::string& s, uint32_t* out) {
  if (s.empty() || s.size() > 3) return false;
  for (char ch : s) {
    if (ch < '0' || ch > '9') return false;
  }
  *out = static_cast<uint32_t>(std::atoi(s.c_str()));
  return true;
}

}  // namespace

bool ApplyParams(const uint8_t* data, size_t size, CaptureConfig* config) {
  if (data == nullptr || size == 0) return true;
  const std::string text(reinterpret_cast<const char*>(data), size);
  bool ok = true;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(pos, end - pos);
    pos = end + 1;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "device") {
      if (value.rfind("/dev/", 0) == 0) {
        config->device = value;
      } else {
        ok = false;
      }
    } else if (key == "input") {
      uint32_t v;
      if (ParseUint(value, &v)) {
        config->input = v;
      } else {
        ok = false;
      }
    } else if (key == "width") {
      if (value == "720" || value == "360") {
        config->width = value == "720" ? 720 : 360;
      } else {
        ok = false;
      }
    } else if (key == "norm") {
      if (value == "pal") {
        config->pal = true;
      } else if (value == "ntsc") {
        config->pal = false;
      } else {
        ok = false;
      }
    }
  }
  return ok;
}

}  // namespace vg
