import 'dart:io';

import 'package:flutter/material.dart';

import 'src/grabber_source.dart';
import 'src/grabber_view.dart';
import 'src/native_grabber_source.dart';
import 'src/test_pattern_source.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  // VG_TEST_PATTERN=1 zeigt Farbbalken statt des Grabbers (Bau und Start
  // prüfen ohne Gerät).
  final GrabberSource source = Platform.environment['VG_TEST_PATTERN'] != null
      ? TestPatternSource()
      : NativeGrabberSource();
  runApp(VideoGrabberApp(source: source));
}

class VideoGrabberApp extends StatefulWidget {
  const VideoGrabberApp({super.key, required this.source});

  final GrabberSource source;

  @override
  State<VideoGrabberApp> createState() => _VideoGrabberAppState();
}

class _VideoGrabberAppState extends State<VideoGrabberApp> {
  @override
  void initState() {
    super.initState();
    widget.source.start();
  }

  @override
  void dispose() {
    widget.source.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Video-Grabber',
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        backgroundColor: Colors.black,
        body: Row(
          children: [
            Expanded(child: GrabberView(source: widget.source)),
            const SizedBox(width: 200, child: TouchProbe()),
          ],
        ),
      ),
    );
  }
}

/// Knopf neben dem Bild: zeigt, ob Berührungen neben der nativen Fläche noch
/// bei Flutter ankommen (Wunsch Michael 01.10.).
class TouchProbe extends StatefulWidget {
  const TouchProbe({super.key});

  @override
  State<TouchProbe> createState() => _TouchProbeState();
}

class _TouchProbeState extends State<TouchProbe> {
  int _taps = 0;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          FilledButton(
            key: const ValueKey('touch-probe'),
            style: FilledButton.styleFrom(minimumSize: const Size(160, 96)),
            onPressed: () => setState(() => _taps++),
            child: const Text('Drück mich', style: TextStyle(fontSize: 22)),
          ),
          const SizedBox(height: 16),
          Text(
            'Berührt: $_taps',
            key: const ValueKey('touch-count'),
            style: const TextStyle(color: Colors.white70, fontSize: 22),
          ),
        ],
      ),
    );
  }
}
