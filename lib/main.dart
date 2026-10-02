import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';

import 'video_grabber.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  // VG_TEST_PATTERN=1 shows colour bars instead of the grabber (to check the
  // build and start-up without a device).
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
    unawaited(widget.source.start());
  }

  @override
  void dispose() {
    unawaited(widget.source.dispose());
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Video Grabber',
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

/// Button next to the picture: shows whether touches beside the native
/// surface still reach Flutter.
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
            child: const Text('Press me', style: TextStyle(fontSize: 22)),
          ),
          const SizedBox(height: 16),
          Text(
            'Touched: $_taps',
            key: const ValueKey('touch-count'),
            style: const TextStyle(color: Colors.white70, fontSize: 22),
          ),
        ],
      ),
    );
  }
}
