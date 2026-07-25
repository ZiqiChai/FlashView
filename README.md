# FlashView

A fast, lightweight image & video viewer for Linux, built with Qt 6.

FlashView is designed for flipping through large folders of pictures with zero
friction: instant paging with neighbour preloading, smooth zoom/fade
animations, a filmstrip thumbnail bar, dark/light themes and full
keyboard/mouse-wheel navigation.

## Features

- **Images**: jpg / jpeg / png / bmp / gif / webp / tiff / tif / svg / ico
- **Videos**: mp4 / mkv / avi / mov / wmv / flv / webm / m4v (via Qt Multimedia + GStreamer)
- Mouse-wheel paging with smooth accumulator (mouse & trackpad friendly)
- Neighbour preloading + pixmap cache for instant back-and-forth browsing
- Non-blocking, progressive thumbnail decoding for large folders
- Natural filename order (`photo2` before `photo10`) and live folder updates
- Smooth zoom animation, cursor-anchored Ctrl+wheel zoom, double-click to
  toggle fit / 100%
- Fade-in transitions between images; auto re-fit on window resize
- Filmstrip thumbnail bar with per-pixel smooth scrolling and centering
- Rotation, fullscreen with idle cursor hiding, drag & drop of files or folders
- Clear empty/error states; stale images and background video audio are never
  left behind when media fails or the active folder changes
- Video playback position, volume and mute controls with remembered volume
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
- GStreamer plugins (runtime, for video playback)

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
scripts/        Dependency installer and build script
tests/          Qt Test regression suite
.github/        CI workflows
```

## License

No license file yet — all rights reserved by the author until one is added.
