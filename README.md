# video_grabber

Flutter-App, die das Live-Bild eines USB-Video-Grabbers (Syntek STK1160, Composite/S-Video, PAL) anzeigt.
Ziel ist der Raspberry Pi 4, gebaut mit [emb_cli](https://pub.dev/packages/emb_cli) und ivi-homescreen
(Backend drm-kms-egl), wie die Karten-App aus `flutter_local_map`. Ton wird nicht genutzt.

Stand und Weg: [docs/plan.md](docs/plan.md).

## Flutter-Version

Gebaut wird mit dem Flutter aus dem emb-Workspace (`~/develop/emb-workspace/flutter`, 3.47.5),
nicht mit dem `flutter` aus dem PATH.
