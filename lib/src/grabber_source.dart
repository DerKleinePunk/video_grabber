import 'package:flutter/widgets.dart';
import 'package:flutter/foundation.dart';

export 'package:flutter/foundation.dart' show ValueListenable;

/// State of a picture source, as the view presents it.
enum GrabberStatus {
  /// The device is being opened, no picture yet.
  connecting,

  /// Frames are arriving.
  playing,

  /// The device is open but delivers no picture (e.g. nothing on the yellow
  /// plug).
  noSignal,

  /// The device is missing (unplugged or driver not loaded).
  deviceMissing,

  /// Opening or streaming failed, reason in [GrabberState.error].
  error,
}

@immutable
class GrabberState {
  const GrabberState(this.status, {this.aspectRatio = 4 / 3, this.error});

  final GrabberStatus status;

  /// Aspect ratio of the picture. PAL from the STK1160 is 4:3.
  final double aspectRatio;

  final String? error;

  @override
  bool operator ==(Object other) =>
      other is GrabberState &&
      other.status == status &&
      other.aspectRatio == aspectRatio &&
      other.error == error;

  @override
  int get hashCode => Object.hash(status, aspectRatio, error);

  @override
  String toString() => 'GrabberState($status, $aspectRatio, $error)';
}

/// A picture source. The real implementation talks to the device, the tests
/// use a fake.
abstract class GrabberSource {
  ValueListenable<GrabberState> get state;

  /// Opens the device and starts showing. May be called again to reconnect
  /// after [GrabberStatus.deviceMissing].
  Future<void> start();

  Future<void> stop();

  /// The picture itself. Only shown while [GrabberStatus.playing].
  Widget buildPicture(BuildContext context);

  Future<void> dispose();
}
