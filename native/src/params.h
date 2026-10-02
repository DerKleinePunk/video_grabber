// Settings Dart passes when creating the view (creationParams): UTF-8 text,
// one "key=value" per line. Known keys: device, input, norm (pal|ntsc),
// width (720|360). Unknown keys are ignored so that new keys do not break
// older libraries.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "v4l2_capture.h"

namespace vg {

// Default: NTSC (the reversing camera sends NTSC).
CaptureConfig DefaultConfig();

// Applies the values from @p data to @p config. Returns false if a known key
// has an invalid value; the remaining values still apply.
bool ApplyParams(const uint8_t* data, size_t size, CaptureConfig* config);

}  // namespace vg
