import 'dart:async';
import 'dart:ffi' as ffi;
import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

import 'grabber_source.dart';

/// View-Art, die native/src/grabber_view.cc anmeldet.
const String grabberViewType = 'video_grabber/view';

/// Was die Quelle von der nativen Bibliothek braucht. Eigene Schnittstelle,
/// damit die Tests ohne Bibliothek auskommen.
abstract class GrabberNativeApi {
  /// Meldet die View-Art an. false, wenn die Bibliothek fehlt.
  bool register();

  /// Zustand der View als GrabberStatus-Index, -1 wenn es sie (noch) nicht gibt.
  int status(int viewId);
}

/// Wo die Bibliothek gesucht wird, in dieser Reihenfolge: VG_LIBRARY, dann
/// über LD_LIBRARY_PATH (so startet carnine-frontend das Bundle aus
/// /opt/carnine/frontend), dann lib/ neben dem Arbeitsordner (Testaufbau).
List<String> libraryCandidates(Map<String, String> environment) => [
  ?environment['VG_LIBRARY'],
  'libvideo_grabber_view.so',
  'lib/libvideo_grabber_view.so',
];

class FfiGrabberNativeApi implements GrabberNativeApi {
  FfiGrabberNativeApi({List<String>? candidates})
    : _candidates = candidates ?? libraryCandidates(Platform.environment);

  final List<String> _candidates;
  int Function(int)? _status;
  String? error;

  ffi.DynamicLibrary _open() {
    Object? last;
    for (final path in _candidates) {
      try {
        return ffi.DynamicLibrary.open(path);
      } on ArgumentError catch (e) {
        last = e;
      }
    }
    throw StateError('${_candidates.join(', ')}: $last');
  }

  @override
  bool register() {
    try {
      final lib = _open();
      lib.lookupFunction<ffi.Int32 Function(), int Function()>('vg_register')();
      _status = lib
          .lookupFunction<ffi.Int32 Function(ffi.Int32), int Function(int)>(
            'vg_status',
          );
      return true;
    } on Object catch (e) {
      error = '$e';
      debugPrint('[video_grabber] $error');
      return false;
    }
  }

  @override
  int status(int viewId) => _status?.call(viewId) ?? -1;
}

/// Bildquelle über die native Platform-View. Der Zustand kommt aus der
/// Aufnahme im nativen Teil und wird regelmäßig abgefragt.
class NativeGrabberSource implements GrabberSource {
  NativeGrabberSource({
    GrabberNativeApi? api,
    this.pollInterval = const Duration(milliseconds: 250),
  }) : api = api ?? FfiGrabberNativeApi();

  final GrabberNativeApi api;
  final Duration pollInterval;

  final ValueNotifier<GrabberState> _state = ValueNotifier(
    const GrabberState(GrabberStatus.connecting),
  );
  bool _registered = false;
  int? _viewId;
  Timer? _timer;

  @override
  ValueListenable<GrabberState> get state => _state;

  @override
  Future<void> start() async {
    _registered = _registered || api.register();
    if (!_registered) {
      _state.value = const GrabberState(
        GrabberStatus.error,
        error: 'Bibliothek video_grabber_view nicht geladen',
      );
      return;
    }
    _timer ??= Timer.periodic(pollInterval, (_) => poll());
  }

  /// Fragt den nativen Zustand ab. Öffentlich für die Tests.
  @visibleForTesting
  void poll() {
    final id = _viewId;
    if (id == null) return;
    final raw = api.status(id);
    if (raw < 0 || raw >= GrabberStatus.values.length) return;
    final next = GrabberState(GrabberStatus.values[raw]);
    if (next != _state.value) _state.value = next;
  }

  /// Von der View gemeldet, sobald der native Teil sie angelegt hat.
  @visibleForTesting
  void attach(int viewId) {
    _viewId = viewId;
    poll();
  }

  @override
  Future<void> stop() async {
    _timer?.cancel();
    _timer = null;
  }

  @override
  Widget buildPicture(BuildContext context) {
    if (!_registered) return const SizedBox.shrink();
    return LayoutBuilder(
      builder: (context, constraints) {
        final dpr = MediaQuery.devicePixelRatioOf(context);
        return PlatformViewLink(
          viewType: grabberViewType,
          surfaceFactory: (context, controller) => PlatformViewSurface(
            controller: controller,
            hitTestBehavior: PlatformViewHitTestBehavior.transparent,
            gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
          ),
          onCreatePlatformView: (params) {
            final controller = _GrabberViewController(
              id: params.id,
              width: constraints.maxWidth * dpr,
              height: constraints.maxHeight * dpr,
            );
            unawaited(
              controller.create().then((_) {
                params.onPlatformViewCreated(params.id);
                attach(params.id);
              }),
            );
            return controller;
          },
        );
      },
    );
  }

  @override
  Future<void> dispose() async {
    await stop();
    _state.dispose();
  }
}

/// Legt die View über den Kanal flutter/platform_views an, wie pv_bench in
/// ivi-homescreen (test/integration/pv_bench/lib/main.dart).
class _GrabberViewController extends PlatformViewController {
  _GrabberViewController({
    required this.id,
    required this.width,
    required this.height,
  });

  final int id;
  final double width;
  final double height;
  bool _created = false;
  Future<void>? _creation;

  @override
  int get viewId => id;

  @override
  bool get awaitingCreation => !_created;

  @override
  Future<void> create({Size? size, Offset? position}) =>
      _creation ??= _createOnce();

  Future<void> _createOnce() async {
    await SystemChannels.platform_views.invokeMethod<void>('create', {
      'id': id,
      'viewType': grabberViewType,
      'direction': 0,
      'width': width,
      'height': height,
    });
    _created = true;
  }

  @override
  Future<void> dispatchPointerEvent(PointerEvent event) async {}

  @override
  Future<void> clearFocus() async {}

  @override
  Future<void> dispose() async {
    if (!_created) return;
    await SystemChannels.platform_views.invokeMethod<void>('dispose', {
      'id': id,
    });
  }
}
