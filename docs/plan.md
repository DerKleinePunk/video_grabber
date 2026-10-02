# Design: Flutter view with the video grabber picture (emb_cli)

## Current design (path C)

A dedicated platform view plugin for ivi-homescreen, **without GStreamer**:

- `native/src/v4l2_capture.*`: V4L2 directly (input, norm, UYVY or YUYV, mmap). Without a source the STK1160
  delivers no frames, so "no signal" is a timeout (`status.*`, 1 s).
- `native/src/uyvy.*`: top field UYVY → NV16 (720x288 PAL, 720x240 NTSC; width 360 optional). USB cameras
  (YUYV, progressive 640x480) are converted as full frames. The vc4 planes of the Pi 4 scan out NV16 directly.
- `native/src/grabber_view.cc`: `libvideo_grabber_view.so` on top of `libihs_shared` (`ihs/platform_view.h`).
  A ring of three DRM dumb buffers, `ihs_pv_submit`. The shell puts the picture on a KMS plane or draws it as a
  texture. `SOFTWARE_SHM` is not wired up under drm-kms-egl, hence dma-buf right away.
- Dart: `NativeGrabberSource` loads the library (`VG_LIBRARY`, then `LD_LIBRARY_PATH`, then
  `lib/libvideo_grabber_view.so`), creates the view like `pv_bench` and polls `vg_status`. Settings come from
  `GrabberConfig` (device, input, norm, width) as creationParams; `VG_DEVICE`, `VG_INPUT`, `VG_NORM` and
  `VG_WIDTH` override them for manual tests.

Verified on a Pi 4 (drm-kms-egl, 1024x600): NV16 negotiated via dma-buf import, view 800x600 (4:3), "no signal"
without a source, live picture from an NTSC reversing camera, unplugging and replugging while running, touch
next to the view, USB camera in colour.

Width 360 is the default: at 720 the STK1160 saturates USB 2.0 and about two thirds of the frames arrive
incomplete; at 360 all 30 frames per second arrive.

Building: see the README.

## Options considered

**Path A: `video_player` with `v4l2:///dev/video0`.** The emb workspace has `video_player_linux` (GStreamer,
`playbin`), which can read the URI directly. Open issues were live sources without duration, setting norm and
input, latency and deinterlacing.

**Path B: own plugin with a fixed GStreamer pipeline** (`v4l2src ! deinterlace ! videoconvert ! appsink`) into a
Flutter texture, modelled on `video_player_linux`.

**Path C (chosen): no GStreamer at all**, V4L2 straight into KMS planes. Fewer dependencies, lowest latency and
CPU load, and full control over norm, input and incomplete frames.

## Hardware notes

- The STK1160 (`05e1:0408`) is an **analogue** grabber (composite/S-Video, PAL 720x576 or NTSC 720x480,
  interlaced), not a UVC webcam. Its audio interface is not used.
- The Raspberry Pi OS kernel ships `stk1160.ko` and `saa7115.ko` (the decoder chip). The WSL kernel lacks
  `CONFIG_VIDEO_STK1160`, so development happens on the Pi.
- Input 0 is Composite0 (yellow plug), input 4 is S-Video. The reversing camera on the test rig sends NTSC;
  the grabber's own norm detection is not reliable for it.

## Tests

- **Dart:** widget tests for the status messages, aspect ratio and "no signal" with a fake source; unit tests
  for `GrabberConfig` and `NativeGrabberSource` with a fake native API.
- **Native:** tests for the repacking, the status tracker, the parameter parser and the capture against a fake
  `Sys` (no device needed); they also run on the Pi.
- **Device:** `native/tools/vg_probe` (frames, rate, incomplete frames) and `tools/drm_shot_planes.py` (screenshot
  across all KMS planes) on the Pi.
