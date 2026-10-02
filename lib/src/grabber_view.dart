import 'package:flutter/material.dart';

import 'grabber_source.dart';

/// Shows the picture of a [GrabberSource] in the right aspect ratio, and a
/// message on black otherwise.
class GrabberView extends StatelessWidget {
  const GrabberView({super.key, required this.source, this.messageBuilder});

  final GrabberSource source;

  /// Replaces the built-in (English) messages, e.g. for translations.
  /// Only called while no picture is playing.
  final String Function(GrabberState state)? messageBuilder;

  static String messageFor(GrabberState state) => switch (state.status) {
    GrabberStatus.connecting => 'Connecting …',
    GrabberStatus.playing => '',
    GrabberStatus.noSignal => 'No signal',
    GrabberStatus.deviceMissing => 'Camera not connected',
    GrabberStatus.error => 'Error: ${state.error ?? 'unknown'}',
  };

  @override
  Widget build(BuildContext context) {
    return ColoredBox(
      color: Colors.black,
      child: ValueListenableBuilder<GrabberState>(
        valueListenable: source.state,
        builder: (context, state, _) {
          // The picture always stays in the tree: for a native source the
          // capture is tied to the view and must survive status changes.
          return Center(
            child: AspectRatio(
              aspectRatio: state.aspectRatio,
              child: Stack(
                fit: StackFit.expand,
                children: [
                  source.buildPicture(context),
                  if (state.status != GrabberStatus.playing)
                    ColoredBox(
                      color: Colors.black,
                      child: Center(
                        child: Text(
                          (messageBuilder ?? messageFor)(state),
                          key: const ValueKey('grabber-message'),
                          style: const TextStyle(
                            color: Colors.white70,
                            fontSize: 28,
                          ),
                          textAlign: TextAlign.center,
                        ),
                      ),
                    ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}
