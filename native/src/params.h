// Einstellungen, die Dart beim Anlegen der View mitgibt (creationParams):
// UTF-8-Text, je Zeile "schlüssel=wert". Bekannt: device, input, norm
// (pal|ntsc). Unbekanntes wird ignoriert, damit neue Schlüssel ältere
// Bibliotheken nicht stören.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "v4l2_capture.h"

namespace vg {

// Vorgabe: NTSC (Michael 01.10.: die Rückfahrkamera sendet NTSC).
CaptureConfig DefaultConfig();

// Übernimmt die Werte aus @p data in @p config. Gibt false zurück, wenn ein
// bekannter Schlüssel einen ungültigen Wert hat; der Rest gilt trotzdem.
bool ApplyParams(const uint8_t* data, size_t size, CaptureConfig* config);

}  // namespace vg
