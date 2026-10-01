// Umsortieren der Grabber-Bilder (UYVY 4:2:2, gepackt) in NV16 (4:2:2,
// Y-Ebene plus verschachtelte CbCr-Ebene). NV16 kann die Display-Hardware des
// Pi 4 (vc4) direkt auf einer Ebene zeigen, UYVY nicht.

#pragma once

#include <cstddef>
#include <cstdint>

namespace vg {

// Welche Zeilen eines Vollbilds übernommen werden. Der STK1160 liefert ein
// Vollbild aus zwei verschachtelten Halbbildern. Für die Rückfahrkamera ist
// ein Halbbild genug und hat keine Kammeffekte.
enum class Field {
  kBoth,    // alle Zeilen (Vollbild, height Zeilen)
  kTop,     // gerade Zeilen 0, 2, 4, ... (height / 2 Zeilen)
  kBottom,  // ungerade Zeilen 1, 3, 5, ... (height / 2 Zeilen)
};

// Zeilen im Ergebnis für @p height Quellzeilen.
uint32_t OutputHeight(uint32_t height, Field field);

// Gepacktes 4:2:2: UYVY (STK1160) oder YUYV (USB-Kameras, uvcvideo).
enum class Packing { kUyvy, kYuyv };

// Wandelt ein gepacktes Bild in NV16 um (Vorgabe UYVY). @p width muss gerade sein. Strides in Bytes:
// UYVY braucht width * 2 je Zeile, Y width, CbCr width.
// Gibt false zurück, wenn ein Argument nicht passt; dann ist nichts
// geschrieben.
bool UyvyToNv16(const uint8_t* src,
                size_t src_stride,
                uint32_t width,
                uint32_t height,
                Field field,
                uint8_t* dst_y,
                size_t y_stride,
                uint8_t* dst_uv,
                size_t uv_stride,
                Packing packing = Packing::kUyvy);

}  // namespace vg
