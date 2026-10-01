import 'package:flutter/material.dart';

import 'src/grabber_source.dart';
import 'src/grabber_view.dart';
import 'src/test_pattern_source.dart';

void main() {
  // Bis die Quelle am Gerät steht (docs/plan.md, Schritt 2): Testbild.
  runApp(VideoGrabberApp(source: TestPatternSource()));
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
      home: Scaffold(body: GrabberView(source: widget.source)),
    );
  }
}
