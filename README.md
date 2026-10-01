# video_grabber

Flutter-App, die das Live-Bild eines USB-Video-Grabbers (Syntek STK1160, Composite/S-Video, PAL) anzeigt.
Ziel ist der Raspberry Pi 4, gebaut mit [emb_cli](https://pub.dev/packages/emb_cli) und ivi-homescreen
(Backend drm-kms-egl), wie die Karten-App aus `flutter_local_map`. Ton wird nicht genutzt.

Stand und Weg: [docs/plan.md](docs/plan.md).

## Flutter-Version

Gebaut wird mit dem Flutter aus dem emb-Workspace (`~/develop/emb-workspace/flutter`, 3.47.5),
nicht mit dem `flutter` aus dem PATH.

## Bauen für den Pi 4

App (emb_cli, aus `~/develop/emb-workspace/app/ivi-homescreen`):

```sh
emb cross . --target rpi4-trixie --build --backend drm-kms-egl \
  --app ~/develop/video_grabber --mode release -D DISABLE_PLUGINS=ON \
  -w ~/develop/emb-workspace
```

Plugin (gegen dasselbe Shell-Build-Verzeichnis, das emb meldet):

```sh
W=~/develop/emb-workspace/.config/flutter_workspace
cmake -S native -B build-pi -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE=$W/cross-aarch64-none-linux-gnu-*/aarch64-none-linux-gnu-<id>-toolchain.cmake \
  -DIHS_BUILD_DIR=$W/cross-build-aarch64-none-linux-gnu-<id>/build-drm-kms-egl \
  -DIHS_SOURCE_DIR=~/develop/emb-workspace/app/ivi-homescreen
cmake --build build-pi
```

`runnable/` von emb und `build-pi/libvideo_grabber_view.so` (nach `lib/`) auf den Pi kopieren, dort
`./homescreen -b . -f`. `VG_TEST_PATTERN=1` zeigt Farbbalken statt des Grabbers.

Tests: `flutter test` (Dart) und `cmake -S native -B build && cmake --build build && ctest --test-dir build` (nativ,
auch auf dem Pi).
