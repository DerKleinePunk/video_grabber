import 'package:flutter/material.dart';

import 'grabber_source.dart';

/// Zeigt das Bild einer [GrabberSource] im richtigen Seitenverhältnis und
/// sonst einen Hinweis auf schwarzem Grund.
class GrabberView extends StatelessWidget {
  const GrabberView({super.key, required this.source});

  final GrabberSource source;

  static String messageFor(GrabberState state) => switch (state.status) {
    GrabberStatus.connecting => 'Verbinde …',
    GrabberStatus.playing => '',
    GrabberStatus.noSignal => 'Kein Signal',
    GrabberStatus.deviceMissing => 'Kamera nicht angeschlossen',
    GrabberStatus.error => 'Fehler: ${state.error ?? 'unbekannt'}',
  };

  @override
  Widget build(BuildContext context) {
    return ColoredBox(
      color: Colors.black,
      child: ValueListenableBuilder<GrabberState>(
        valueListenable: source.state,
        builder: (context, state, _) {
          return Center(
            child: AspectRatio(
              aspectRatio: state.aspectRatio,
              child: state.status == GrabberStatus.playing
                  ? source.buildPicture(context)
                  : Center(
                      child: Text(
                        messageFor(state),
                        key: const ValueKey('grabber-message'),
                        style: const TextStyle(
                          color: Colors.white70,
                          fontSize: 28,
                        ),
                        textAlign: TextAlign.center,
                      ),
                    ),
            ),
          );
        },
      ),
    );
  }
}
