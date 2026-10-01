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
        body: GrabberView(source: widget.source),
      ),
    );
  }
}
