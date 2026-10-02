import 'package:flutter_test/flutter_test.dart';
import 'package:video_grabber/src/grabber_source.dart';
import 'package:video_grabber/src/native_grabber_source.dart';

class FakeApi implements GrabberNativeApi {
  FakeApi({this.loads = true});
  final bool loads;
  int registers = 0;
  final Map<int, int> statuses = {};

  @override
  bool register() {
    registers++;
    return loads;
  }

  @override
  int status(int viewId) => statuses[viewId] ?? -1;
}

void main() {
  test('missing library is reported as an error', () async {
    final source = NativeGrabberSource(api: FakeApi(loads: false));
    await source.start();
    expect(source.state.value.status, GrabberStatus.error);
    expect(source.state.value.error, contains('video_grabber_view'));
    await source.dispose();
  });

  test('takes the native state of the created view', () async {
    final api = FakeApi();
    final source = NativeGrabberSource(api: api);
    await source.start();
    expect(api.registers, 1);
    expect(source.state.value.status, GrabberStatus.connecting);

    api.statuses[7] = 2;
    source.attach(7);
    expect(source.state.value.status, GrabberStatus.noSignal);

    api.statuses[7] = 1;
    source.poll();
    expect(source.state.value.status, GrabberStatus.playing);

    api.statuses[7] = 3;
    source.poll();
    expect(source.state.value.status, GrabberStatus.deviceMissing);
    await source.dispose();
  });

  test('unknown values and a missing view change nothing', () async {
    final api = FakeApi();
    final source = NativeGrabberSource(api: api);
    await source.start();
    source.poll(); // no view yet
    expect(source.state.value.status, GrabberStatus.connecting);
    api.statuses[1] = 99;
    source.attach(1);
    expect(source.state.value.status, GrabberStatus.connecting);
    api.statuses[1] = -1;
    source.poll();
    expect(source.state.value.status, GrabberStatus.connecting);
    await source.dispose();
  });

  test('polls regularly and stops after stop()', () async {
    final api = FakeApi();
    final source = NativeGrabberSource(
      api: api,
      pollInterval: const Duration(milliseconds: 10),
    );
    await source.start();
    api.statuses[3] = 0;
    source.attach(3);
    api.statuses[3] = 1;
    await Future<void>.delayed(const Duration(milliseconds: 40));
    expect(source.state.value.status, GrabberStatus.playing);

    await source.stop();
    api.statuses[3] = 2;
    await Future<void>.delayed(const Duration(milliseconds: 40));
    expect(source.state.value.status, GrabberStatus.playing);
    await source.dispose();
  });

  test('library: VG_LIBRARY first, then LD_LIBRARY_PATH, then lib/', () {
    expect(libraryCandidates({'VG_LIBRARY': '/x/y.so'}), [
      '/x/y.so',
      'libvideo_grabber_view.so',
      'lib/libvideo_grabber_view.so',
    ]);
    expect(libraryCandidates({}), [
      'libvideo_grabber_view.so',
      'lib/libvideo_grabber_view.so',
    ]);
  });

  test('order of the states matches native/src/status.h', () {
    expect(GrabberStatus.values.map((s) => s.name).toList(), [
      'connecting',
      'playing',
      'noSignal',
      'deviceMissing',
      'error',
    ]);
  });
}
