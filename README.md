# video_grabber

Flutter app and library that shows the live picture of a USB video grabber (Syntek STK1160, composite/S-Video,
PAL/NTSC) or a USB camera (uvcvideo, YUYV). The target is the Raspberry Pi 4, built with
[emb_cli](https://pub.dev/packages/emb_cli) and ivi-homescreen (backend drm-kms-egl), like the map app from
`flutter_local_map`. Audio is not used.

Design and history: [docs/plan.md](docs/plan.md).

## Flutter version

Build with the Flutter from the emb workspace (`~/develop/emb-workspace/flutter`, 3.47.5), not with the `flutter`
on the PATH.

## Building for the Pi 4

App (emb_cli, from `~/develop/emb-workspace/app/ivi-homescreen`):

```sh
emb cross . --target rpi4-trixie --build --backend drm-kms-egl \
  --app ~/develop/video_grabber --mode release -D DISABLE_PLUGINS=ON \
  -w ~/develop/emb-workspace
```

Plugin (against the same shell build directory that emb reports):

```sh
W=~/develop/emb-workspace/.config/flutter_workspace
cmake -S native -B build-pi -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_TOOLCHAIN_FILE=$W/cross-aarch64-none-linux-gnu-*/aarch64-none-linux-gnu-<id>-toolchain.cmake \
  -DIHS_BUILD_DIR=$W/cross-build-aarch64-none-linux-gnu-<id>/build-drm-kms-egl \
  -DIHS_SOURCE_DIR=~/develop/emb-workspace/app/ivi-homescreen
cmake --build build-pi
```

Copy emb's `runnable/` and `build-pi/libvideo_grabber_view.so` (into `lib/`) to the Pi and run
`./homescreen -b . -f` there. `VG_TEST_PATTERN=1` shows colour bars instead of the grabber.

Build output (`libapp.so*`, `build/`, `build-pi/`, `native/build/`) is never committed.

## Tests

- Dart: `flutter test`
- Native (also on the Pi): `cmake -S native -B build && cmake --build build && ctest --test-dir build`
