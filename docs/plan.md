# Entwurf: Flutter-Fenster mit dem Bild des Video-Grabbers (emb_cli)

Stand: 01.10.2026, flutter-local-map. Auftrag: 2026-10-01_0901_michael_video-grabber-projekt.md. Ton bleibt außen vor.

## Was ich heute gefunden habe (geprüft)

- **Grabber:** `lsusb` auf DELEWS004 (WSL) zeigt `05e1:0408 Syntek STK1160 Video Capture Device`, „USB 2.0 Video Capture Controller“.
  - Das ist ein **analoger** Grabber (Composite/S-Video, PAL 720x576, Halbbilder), keine Webcam (kein UVC).
  - Er hat eine Audio-Schnittstelle, die wir nicht nutzen.
- **WSL kann ihn nicht ansprechen:** Der WSL-Kernel 6.18.33.2 hat `CONFIG_VIDEO_STK1160` nicht, es gibt kein `/dev/video*`. V4L2 selbst ist als Modul da.
- **Der Pi kann es:** Der Kernel auf jeep-pi (6.18.50+rpt-rpi-v8) bringt `stk1160.ko` und `saa7115.ko` mit (den Decoder-Chip im Grabber).
- **emb_cli / ivi-homescreen:** Im Workspace `~/develop/emb-workspace` liegt das Plugin **`video_player_linux`** (GStreamer, `playbin`). Es ist für das pub-Paket `video_player` gedacht und rendert in eine Flutter-Textur.
  - GStreamer kennt die URI `v4l2:///dev/video0`. `playbin` kann also im Prinzip direkt vom Grabber lesen.
  - Das `camera`-Plugin ist laut README noch „WIP“ und hängt an libcamera, das passt nicht zu einem V4L2-Grabber.

## Ziel

Eine kleine Flutter-App für den Pi 4, gebaut mit emb_cli wie die Karten-App (drm-kms-egl). Sie zeigt das Live-Bild des Grabbers in einem Fenster bzw. Widget, mit:
- Seitenverhältnis 4:3,
- dem Hinweis „Kein Signal“, wenn nichts anliegt,
- Wiederverbinden, wenn der Grabber abgezogen und wieder angesteckt wird.

Später kann daraus ein Widget/Paket für carnine2 werden (z. B. Rückfahrkamera), das ist aber nicht Teil dieses Plans.

## Weg

**Schritt 0: Grabber am Pi in Betrieb nehmen (ohne Flutter, ca. 1 h).**
- Den Grabber an jeep-pi stecken (Michael), Kamera oder Quelle an Composite.
- Prüfen:
  - `dmesg`, ob `stk1160` und `saa7115` laden,
  - `/dev/video0`,
  - `v4l2-ctl --list-formats-ext`, `--list-inputs`, Norm PAL.
- Ein Standbild mit `gst-launch-1.0 v4l2src norm=PAL ! videoconvert ! jpegenc ! filesink` aufnehmen und auf den Share legen.
- Es fehlen ggf. die Pakete `v4l-utils`, `gstreamer1.0-plugins-good` (v4l2src) und `-base`.
- Ergebnis: Format, Auflösung, Bildrate und wie das Bild aussieht (Halbbilder, Ränder).

**Schritt 1: App-Gerüst `video_grabber` (lokal, emb_cli).**
- Neues Flutter-Projekt.
- Baueintrag im emb-Workspace wie `map_local_pi` (`emb cross … --backend drm-kms-egl`), diesmal **mit** dem Plugin `video_player_linux` (nicht `DISABLE_PLUGINS=ON`).
- Erst nur ein Testbild, um Bau und Start auf jeep-pi zu klären.

**Schritt 2, Weg A (zuerst versuchen): `video_player` mit `v4l2:///dev/video0`.**
- `VideoPlayerController.networkUrl(Uri.parse('v4l2:///dev/video0'))`, kein eigener nativer Code.
- Offene Punkte:
  - Wie geht `playbin` mit einer Live-Quelle um (kein Ende, keine Dauer)?
  - Wie stellt man die PAL-Norm und den Eingang ein? Ggf. über `v4l2-ctl` vor dem Start.
  - Wie groß ist die Verzögerung?
  - Gibt es einen Deinterlacer (sonst Kammeffekt)?
- Wenn das reicht: fertig mit dem kleinsten Aufwand.

**Schritt 2, Weg B (falls A hakt): eigenes kleines Plugin im ivi-homescreen.**
- Eine feste GStreamer-Kette `v4l2src norm=PAL device=… ! deinterlace ! videoconvert ! appsink` (RGBA oder NV12), Bild in eine Flutter-Textur.
- Der Aufbau ist wie bei `video_player_linux`, dessen Textur-Code (inkl. `nv12.h`) übernehme ich.
- Ein MethodChannel steuert `start/stop`, Gerät, Norm, Eingang und meldet den Status „kein Signal“.
- Mehr Arbeit (geschätzt 2–3 Tage), dafür bestimmen wir die Kette selbst und haben weniger Verzögerung.

**Schritt 3: Messen auf jeep-pi.**
- CPU-Last, RSS und Verzögerung (Uhr ins Bild halten und abfotografieren).
- Abziehen und Anstecken im Betrieb, Dauerlauf 1 h.
- Temperatur (siehe #70).

## Tests

- **Dart:** Widget-Tests für Statusanzeige, Seitenverhältnis und „Kein Signal“, mit einem nachgebildeten Controller.
- **Nativ (Weg B):** GoogleTest wie bei den anderen ivi-Plugins, Kette mit `videotestsrc` statt `v4l2src`, ohne Gerät.
- **Gerät:** Schritt 0 und 3 auf jeep-pi, mit Bildern als Nachweis.

## Offene Fragen an Michael

1. **Name und Ort:** Vorschlag `~/develop/video_grabber`, eigenes GitHub-Repo `DerKleinePunk/video_grabber`? Oder als Beispiel-App in flutter_local_map?
2. **Zum Testen am Pi:** Darf der Grabber (und eine Quelle) an jeep-pi? Lokal in WSL geht es nur mit einem selbst gebauten Kernelmodul für den WSL-Kernel. Das würde ich vermeiden, getestet und freigegeben wird ohnehin nur der Pi 4.
3. **Wofür später?** Rückfahrkamera, DVB-T oder etwas anderes? Das entscheidet, ob die Verzögerung wichtig ist (bei der Rückfahrkamera ja).
4. **Quelle:** Was hängt am Grabber, PAL-Composite?

Bis zur Antwort lege ich nichts an und baue nichts.
