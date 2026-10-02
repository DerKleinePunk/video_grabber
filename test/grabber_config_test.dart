import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:video_grabber/video_grabber.dart';

void main() {
  test('defaults to NTSC on Composite0 of /dev/video0', () {
    const c = GrabberConfig();
    expect(c.norm, VideoNorm.ntsc);
    expect(c.input, 0);
    expect(c.device, '/dev/video0');
    expect(c.width, 360);
  });

  test('encodes as native/src/params.h expects', () {
    const c = GrabberConfig(
      device: '/dev/video2',
      input: 4,
      norm: VideoNorm.pal,
      width: 720,
    );
    expect(
      utf8.decode(c.encode()),
      'device=/dev/video2\ninput=4\nnorm=pal\nwidth=720\n',
    );
    expect(
      utf8.decode(const GrabberConfig().encode()),
      'device=/dev/video0\ninput=0\nnorm=ntsc\nwidth=360\n',
    );
  });

  test('equality for ValueKey', () {
    expect(const GrabberConfig(input: 1), const GrabberConfig(input: 1));
    expect(
      const GrabberConfig(norm: VideoNorm.pal),
      isNot(const GrabberConfig()),
    );
    expect(const GrabberConfig(width: 720), isNot(const GrabberConfig()));
  });

  test('NativeGrabberSource takes the settings', () {
    final s = NativeGrabberSource(
      config: const GrabberConfig(norm: VideoNorm.pal),
    );
    expect(s.config.norm, VideoNorm.pal);
    expect(NativeGrabberSource().config, const GrabberConfig());
  });
}
