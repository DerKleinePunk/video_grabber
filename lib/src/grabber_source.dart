import 'package:flutter/widgets.dart';
import 'package:flutter/foundation.dart';

export 'package:flutter/foundation.dart' show ValueListenable;

/// Zustand einer Bildquelle, so wie die Anzeige ihn darstellt.
enum GrabberStatus {
  /// Gerät wird geöffnet, noch kein Bild.
  connecting,

  /// Es kommen Bilder.
  playing,

  /// Das Gerät ist offen, liefert aber kein Bild (z. B. nichts am gelben Stecker).
  noSignal,

  /// Das Gerät fehlt (abgezogen oder Treiber nicht geladen).
  deviceMissing,

  /// Öffnen oder Abspielen ist fehlgeschlagen, Grund in [GrabberState.error].
  error,
}

@immutable
class GrabberState {
  const GrabberState(this.status, {this.aspectRatio = 4 / 3, this.error});

  final GrabberStatus status;

  /// Seitenverhältnis des Bildes. PAL vom STK1160 ist 4:3.
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

/// Eine Bildquelle. Die echte Umsetzung hängt am Gerät, die Tests nehmen
/// eine nachgebildete.
abstract class GrabberSource {
  ValueListenable<GrabberState> get state;

  /// Öffnet das Gerät und beginnt zu zeigen. Darf erneut aufgerufen werden,
  /// um nach [GrabberStatus.deviceMissing] wieder zu verbinden.
  Future<void> start();

  Future<void> stop();

  /// Das Bild selbst. Wird nur bei [GrabberStatus.playing] eingebaut.
  Widget buildPicture(BuildContext context);

  Future<void> dispose();
}
