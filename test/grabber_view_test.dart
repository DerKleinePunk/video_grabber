import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:video_grabber/main.dart';
import 'package:video_grabber/src/grabber_source.dart';
import 'package:video_grabber/src/grabber_view.dart';
import 'package:video_grabber/src/test_pattern_source.dart';

class FakeSource implements GrabberSource {
  final ValueNotifier<GrabberState> notifier = ValueNotifier(
    const GrabberState(GrabberStatus.connecting),
  );
  int starts = 0;
  bool disposed = false;

  @override
  ValueListenable<GrabberState> get state => notifier;

  @override
  Future<void> start() async => starts++;

  @override
  Future<void> stop() async {}

  @override
  Widget buildPicture(BuildContext context) =>
      const ColoredBox(key: ValueKey('picture'), color: Colors.red);

  @override
  Future<void> dispose() async => disposed = true;
}

Widget _wrap(GrabberSource source, {Size size = const Size(1024, 600)}) =>
    MaterialApp(
      home: MediaQuery(
        data: MediaQueryData(size: size),
        child: Scaffold(body: GrabberView(source: source)),
      ),
    );

String _message(WidgetTester tester) =>
    tester.widget<Text>(find.byKey(const ValueKey('grabber-message'))).data!;

void main() {
  testWidgets('zeigt je Zustand den passenden Hinweis und kein Bild', (
    tester,
  ) async {
    final source = FakeSource();
    await tester.pumpWidget(_wrap(source));

    final expected = {
      GrabberStatus.connecting: 'Verbinde …',
      GrabberStatus.noSignal: 'Kein Signal',
      GrabberStatus.deviceMissing: 'Kamera nicht angeschlossen',
    };
    for (final entry in expected.entries) {
      source.notifier.value = GrabberState(entry.key);
      await tester.pump();
      expect(_message(tester), entry.value);
    }

    source.notifier.value = const GrabberState(
      GrabberStatus.error,
      error: 'v4l2src: busy',
    );
    await tester.pump();
    expect(_message(tester), 'Fehler: v4l2src: busy');
  });

  testWidgets(
    'Bild bleibt eingebaut, der Hinweis liegt nur ohne Bild darüber',
    (tester) async {
      final source = FakeSource();
      await tester.pumpWidget(_wrap(source));
      final picture = find.byKey(const ValueKey('picture'));
      expect(picture, findsOneWidget);
      final element = tester.element(picture);
      expect(find.byKey(const ValueKey('grabber-message')), findsOneWidget);

      source.notifier.value = const GrabberState(GrabberStatus.playing);
      await tester.pump();
      expect(find.byKey(const ValueKey('grabber-message')), findsNothing);
      // Dieselbe View, nicht neu angelegt: die Aufnahme hängt daran.
      expect(tester.element(picture), same(element));

      source.notifier.value = const GrabberState(GrabberStatus.noSignal);
      await tester.pump();
      expect(find.byKey(const ValueKey('grabber-message')), findsOneWidget);
      expect(tester.element(picture), same(element));
    },
  );

  testWidgets('hält 4:3 auf dem 1024x600-Bildschirm ein', (tester) async {
    tester.view.physicalSize = const Size(1024, 600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);

    final source = FakeSource()
      ..notifier.value = const GrabberState(GrabberStatus.playing);
    await tester.pumpWidget(_wrap(source));

    final size = tester.getSize(find.byKey(const ValueKey('picture')));
    expect(size.width / size.height, closeTo(4 / 3, 0.001));
    expect(size.height, 600);
    expect(size.width, 800);
  });

  testWidgets('App startet die Quelle und gibt sie wieder frei', (
    tester,
  ) async {
    final source = FakeSource();
    await tester.pumpWidget(VideoGrabberApp(source: source));
    expect(source.starts, 1);

    await tester.pumpWidget(const SizedBox());
    expect(source.disposed, isTrue);
  });

  testWidgets('Testbild zeigt die Farbbalken', (tester) async {
    final source = TestPatternSource();
    await tester.pumpWidget(VideoGrabberApp(source: source));
    await tester.pump();
    expect(find.byKey(const ValueKey('test-pattern')), findsOneWidget);
  });

  testWidgets('Knopf neben dem Bild zählt Berührungen', (tester) async {
    tester.view.physicalSize = const Size(1024, 600);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.reset);
    final source = FakeSource()
      ..notifier.value = const GrabberState(GrabberStatus.playing);
    await tester.pumpWidget(VideoGrabberApp(source: source));

    final picture = tester.getRect(find.byKey(const ValueKey('picture')));
    final button = tester.getRect(find.byKey(const ValueKey('touch-probe')));
    expect(picture.overlaps(button), isFalse);
    expect(button.left, greaterThanOrEqualTo(picture.right));
    // 4:3 im Rest neben der 200 px breiten Leiste.
    expect(picture.width / picture.height, closeTo(4 / 3, 0.01));

    expect(find.text('Berührt: 0'), findsOneWidget);
    await tester.tap(find.byKey(const ValueKey('touch-probe')));
    await tester.tap(find.byKey(const ValueKey('touch-probe')));
    await tester.pump();
    expect(find.text('Berührt: 2'), findsOneWidget);
  });

  testWidgets('eigene Hinweise ersetzen die eingebauten', (tester) async {
    final source = FakeSource();
    await tester.pumpWidget(
      MaterialApp(
        home: GrabberView(
          source: source,
          messageBuilder: (s) => 'EN:${s.status.name}:${s.error ?? ''}',
        ),
      ),
    );
    expect(_message(tester), 'EN:connecting:');
    source.notifier.value = const GrabberState(GrabberStatus.noSignal);
    await tester.pump();
    expect(_message(tester), 'EN:noSignal:');
    source.notifier.value = const GrabberState(
      GrabberStatus.error,
      error: 'busy',
    );
    await tester.pump();
    expect(_message(tester), 'EN:error:busy');
    source.notifier.value = const GrabberState(GrabberStatus.playing);
    await tester.pump();
    expect(find.byKey(const ValueKey('grabber-message')), findsNothing);
  });
}
