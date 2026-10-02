import 'package:flutter/material.dart';

import 'grabber_source.dart';

/// Colour bars instead of a camera picture. Lets you check the build and
/// start-up on the Pi before the grabber is connected.
class TestPatternSource implements GrabberSource {
  final ValueNotifier<GrabberState> _state = ValueNotifier(
    const GrabberState(GrabberStatus.connecting),
  );

  @override
  ValueListenable<GrabberState> get state => _state;

  @override
  Future<void> start() async {
    _state.value = const GrabberState(GrabberStatus.playing);
  }

  @override
  Future<void> stop() async {
    _state.value = const GrabberState(GrabberStatus.connecting);
  }

  static const bars = [
    Colors.white,
    Colors.yellow,
    Colors.cyan,
    Colors.green,
    Colors.purple,
    Colors.red,
    Colors.blue,
    Colors.black,
  ];

  @override
  Widget buildPicture(BuildContext context) => Row(
    key: const ValueKey('test-pattern'),
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [for (final c in bars) Expanded(child: ColoredBox(color: c))],
  );

  @override
  Future<void> dispose() async => _state.dispose();
}
