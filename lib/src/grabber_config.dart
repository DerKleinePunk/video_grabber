import 'dart:convert';

import 'package:flutter/foundation.dart';

enum VideoNorm { pal, ntsc }

/// Camera settings. Norm, input and width apply to the grabber (STK1160,
/// UYVY); the plugin detects a USB camera (uvcvideo, YUYV) by itself and uses
/// 640x480 progressive, where only [device] matters.
/// The settings are not stored here but by the backend (carnine2); the app
/// only passes them on to the native view.
@immutable
class GrabberConfig {
  const GrabberConfig({
    this.device = '/dev/video0',
    this.input = 0,
    this.norm = VideoNorm.ntsc,
    this.width = 360,
  }) : assert(width == 360 || width == 720);

  /// V4L2 device of the grabber.
  final String device;

  /// Input: 0 = Composite0 (yellow plug), 4 = S-Video on the STK1160.
  final int input;

  /// The reversing camera on the test rig sends NTSC; the grabber's own
  /// detection is not reliable for it.
  final VideoNorm norm;

  /// Picture width from the grabber: 360 or 720. At 720 USB is at its limit
  /// (on a Pi 4 ~2/3 of the frames arrive incomplete, ~10/s visible); at 360
  /// all 30/s arrive. For an analogue camera picture 360 costs little
  /// sharpness.
  final int width;

  /// creationParams for the native view, format as in native/src/params.h.
  Uint8List encode() => Uint8List.fromList(
    utf8.encode(
      'device=$device\ninput=$input\nnorm=${norm.name}\nwidth=$width\n',
    ),
  );

  @override
  bool operator ==(Object other) =>
      other is GrabberConfig &&
      other.device == device &&
      other.input == input &&
      other.norm == norm &&
      other.width == width;

  @override
  int get hashCode => Object.hash(device, input, norm, width);

  @override
  String toString() => 'GrabberConfig($device, $input, ${norm.name}, $width)';
}
