import 'dart:convert';

import 'package:flutter/foundation.dart';

enum VideoNorm { pal, ntsc }

/// Einstellungen der Kamera. Gespeichert werden sie nicht hier, sondern vom
/// Backend (carnine2); die App reicht sie nur an die native View weiter.
@immutable
class GrabberConfig {
  const GrabberConfig({
    this.device = '/dev/video0',
    this.input = 0,
    this.norm = VideoNorm.ntsc,
  });

  /// V4L2-Gerät des Grabbers.
  final String device;

  /// Eingang: 0 = Composite0 (gelber Stecker), 4 = S-Video beim STK1160.
  final int input;

  /// Die Rückfahrkamera am Testaufbau sendet NTSC; die Erkennung des
  /// Grabbers ist dafür nicht verlässlich.
  final VideoNorm norm;

  /// creationParams für die native View, Format wie native/src/params.h.
  Uint8List encode() => Uint8List.fromList(
    utf8.encode('device=$device\ninput=$input\nnorm=${norm.name}\n'),
  );

  @override
  bool operator ==(Object other) =>
      other is GrabberConfig &&
      other.device == device &&
      other.input == input &&
      other.norm == norm;

  @override
  int get hashCode => Object.hash(device, input, norm);

  @override
  String toString() => 'GrabberConfig($device, $input, ${norm.name})';
}
