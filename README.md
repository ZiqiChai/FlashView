# FlashView

[English](README.md) | [简体中文](README.zh-CN.md) · [Changelog](CHANGELOG.md)

A fast, lightweight image & video viewer for Linux, built with Qt 6.

FlashView is designed for flipping through large folders of pictures with zero
friction: instant paging with neighbour preloading, smooth zoom/fade
animations, a filmstrip thumbnail bar, dark/light themes and full
keyboard/mouse-wheel navigation.

## Screenshots

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="screenshots/flashview-dark.png">
    <source media="(prefers-color-scheme: light)" srcset="screenshots/flashview-light.png">
    <img src="screenshots/flashview-dark.png" alt="FlashView main window" width="900">
  </picture>
</p>

<p align="center">
  <img src="screenshots/flashview-view-menu.png" alt="FlashView view menu and unified icon set" width="900">
</p>

## Features

- **Images**: formats are discovered from the installed Qt decoders at runtime;
  common formats include JPEG, PNG, BMP, animated GIF, WebP, TIFF, SVG and ICO,
  with APNG, AVIF, HEIF/HEIC, JPEG XL and camera RAW available when matching
  Qt image plugins are installed
- **Videos**: extensions come from the system MIME database and files are
  content-probed; playback uses Qt Multimedia + the installed GStreamer codecs
- Mouse-wheel paging with smooth accumulator (mouse & trackpad friendly)
- Neighbour preloading + pixmap cache for instant back-and-forth browsing
- Non-blocking, progressive thumbnail decoding for large folders
- Natural filename order (`photo2` before `photo10`) and live folder updates
- Smooth zoom animation, cursor-anchored Ctrl+wheel zoom, double-click to
  toggle fit / 100%
- Automatic, smooth-photo and nearest-neighbor pixel-art scaling modes;
  high-quality static-image resampling runs in the background after zooming
  stops, with bounded memory use and stale-result protection
- Fade-in transitions between images; auto re-fit on window resize without
  upscaling low-resolution images above their pixel-perfect 100% size
- Filmstrip thumbnail bar with per-pixel smooth scrolling and centering
- Rotation, fullscreen with idle cursor hiding, drag & drop of files or folders
- Clear empty/error states; stale images and background video audio are never
  left behind when media fails or the active folder changes
- Video playback position, volume and mute controls with remembered volume
- Consistent vector-style toolbar/menu icons with balanced HiDPI proportions
- Dark / light themes, English & Chinese UI

## Keyboard & mouse

| Action | Shortcut |
| --- | --- |
| Next / previous file | `→` / `←`, mouse wheel, mouse back/forward buttons |
| First / last file | `Home` / `End` |
| Page through files | `PageDown` / `PageUp` |
| Zoom in / out | `Ctrl++` / `Ctrl+-`, `Ctrl` + wheel |
| Fit to window | `F` |
| Actual size (100%) | `1` (or click the zoom % in the status bar) |
| Rotate left / right | `Ctrl+L` / `Ctrl+R` |
| Play / pause video | `Space` |
| Adjust video volume | `Ctrl` + mouse wheel |
| Fullscreen | `F11` |
| Thumbnail bar | `T` |
| Open file / directory | `Ctrl+O` / `Ctrl+Shift+O` |
| Settings | `Ctrl+,` |

## Dependencies

- CMake ≥ 3.16, a C++17 compiler
- Qt 6: Core, Gui, Widgets, Concurrent, Multimedia, MultimediaWidgets,
  LinguistTools and Test
- Qt SVG and image-format runtime plugins
- GStreamer Base, Good, Bad, Ugly and libav plugins (runtime video codecs)

On Debian/Ubuntu (22.04 / 24.04 / 26.04) everything can be installed with:

```bash
./scripts/install-deps.sh        # interactive
./scripts/install-deps.sh -y     # non-interactive (CI)
```

## Building

```bash
./scripts/build.sh               # Release build into ./build
./scripts/build.sh --debug       # Debug build
./scripts/build.sh --clean       # wipe the build directory first
```

Or manually:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/flashview [file_or_directory]
```

To regenerate the documentation screenshots from the real Qt UI:

```bash
QT_QPA_PLATFORM=offscreen \
FLASHVIEW_SCREENSHOT_DIR="$PWD/screenshots" \
./build/flashview_tests generateDocumentationScreenshots
```

## Installing

```bash
./install.sh                     # build + install to /usr/local (uses sudo if needed)
./install.sh --prefix ~/.local   # per-user install, no sudo
./install.sh --deps              # install dependencies first, then build & install
./install.sh --uninstall         # remove installed files
```

This installs the `flashview` binary, a desktop entry and the application
icon, so FlashView shows up in your application menu.

## Continuous integration

Every push and pull request runs the Qt regression suite, an offscreen launch
smoke test and an install-layout check on Ubuntu 22.04, 24.04 and 26.04 via
GitHub Actions — see [.github/workflows/build.yml](.github/workflows/build.yml).

## Project layout

```bash
src/            C++ sources (MainWindow, ImageViewer, VideoPlayer,
                ThumbnailBar, ThemeManager, SettingsDialog)
i18n/           Qt Linguist translations (en, zh)
resources/      Icons and Qt resource files
screenshots/    Reproducible dark/light UI captures
scripts/        Dependency installer and build script
tests/          Qt Test regression suite
.github/        CI workflows
CHANGELOG.md    User-visible change history
```

## License

No license file yet — all rights reserved by the author until one is added.
