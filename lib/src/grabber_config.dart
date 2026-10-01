import 'dart:convert';

import 'package:flutter/foundation.dart';

enum VideoNorm { pal, ntsc }

/// Einstellungen der Kamera. Norm, Eingang und Breite gelten für den Grabber
/// (STK1160, UYVY); eine USB-Kamera (uvcvideo, YUYV) erkennt das Plugin selbst
/// und nimmt 640x480 als Vollbild, dort zählt nur [device].
/// Gespeichert werden sie nicht hier, sondern vom
/// Backend (carnine2); die App reicht sie nur an die native View weiter.
@immutable
class GrabberConfig {
  const GrabberConfig({
    this.device = '/dev/video0',
    this.input = 0,
    this.norm = VideoNorm.ntsc,
    this.width = 360,
  }) : assert(width == 360 || width == 720);

  /// V4L2-Gerät des Grabbers.
  final String device;

  /// Eingang: 0 = Composite0 (gelber Stecker), 4 = S-Video beim STK1160.
  final int input;

  /// Die Rückfahrkamera am Testaufbau sendet NTSC; die Erkennung des
  /// Grabbers ist dafür nicht verlässlich.
  final VideoNorm norm;

  /// Bildbreite vom Grabber: 360 oder 720. Bei 720 ist der USB am Anschlag
  /// (auf jeep-pi ~2/3 der Bilder unvollständig, sichtbar ~10/s), bei 360
  /// kommen alle 30/s. Für ein analoges Kamerabild kostet 360 kaum Schärfe.
  final int width;

  /// creationParams für die native View, Format wie native/src/params.h.
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
