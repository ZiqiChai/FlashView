# FlashView

[English](README.md) | [简体中文](README.zh-CN.md) · [Changelog](CHANGELOG.md)

FlashView is a lightweight, fast, polished and broadly compatible image and
video viewer for Linux, built with Qt 6. Browse a folder, check an image or
video, and move on without waiting for a heavy editor to open. With **Quick
Look**, pressing `Space` on a selected image or video in GNOME Files opens an
instant preview.

## Product characteristics

| Characteristic | What it feels like |
| --- | --- |
| **Lightweight** | Starts quickly, stays focused on viewing, and keeps video and high-quality processing work on demand. |
| **Fast** | Makes folder browsing feel immediate with natural ordering, quick thumbnails, caching and nearby-image preloading. |
| **Beautiful** | Offers a clean toolbar, balanced icons, dark/light themes, smooth transitions and a distraction-free fullscreen view. |
| **Compatible** | Uses the image handlers and video codecs available on the system instead of limiting the viewer to a small built-in format list. |

## Screenshots

The following captures show the **English interface**. Both themes are kept in
the repository, together with matching View-menu captures.

### Dark theme

![FlashView English interface — dark theme](screenshots/flashview-en-dark.png)

### Light theme

![FlashView English interface — light theme](screenshots/flashview-en-light.png)

<details>
<summary>View menu in both themes</summary>

![English View menu — dark theme](screenshots/flashview-en-dark-menu.png)

![English View menu — light theme](screenshots/flashview-en-light-menu.png)

</details>

## Quick start

On Debian/Ubuntu, run these two commands from the repository root:

```bash
./install.sh --deps --prefix ~/.local
~/.local/bin/flashview /path/to/image-or-folder
```

If the dependencies are already installed, omit `--deps`. You can also drag a
file or folder onto the window; `Ctrl+O` opens a file and `Ctrl+Shift+O` opens a
folder.

## Features

### Quick Look: inspect before opening

When the optional preview service is installed, select an image or video in
GNOME Files and press `Space`. FlashView displays a borderless overlay, keeps
the file manager's own selection order, and supports:

| Quick Look action | Shortcut |
| --- | --- |
| Close | `Space` or `Esc` |
| Ask the file manager for the previous/next selection | `←` `→` `↑` `↓` |
| Open in the full FlashView window | `Enter` |
| Fit image / show actual size | `F` / `1` |

The preview service starts when needed and exits after ten idle minutes. It
previews local files only. If the preview service is unavailable, the main
FlashView viewer is unaffected. On Wayland, the overlay is centered instead of
being attached to the file-manager window. Unsupported, missing or remote files
receive an explanation and can still be handed to another application with
`Enter`.

Disable the optional integration with either `./install.sh --no-previewer` or
`-DFLASHVIEW_ENABLE_PREVIEWER=OFF`.

### Fast folder browsing

- Supported files are discovered from the installed Qt image handlers and the
  system MIME database, with content probing for files whose suffix is missing
  or misleading.
- The default **natural filename order** puts `image2` before `image10`.
  Settings and the View menu can pin name, modified time, created time, file
  size or file type, each ascending or descending.
- The current folder and current file are watched for additions, removals and
  in-place replacements. The current item is preserved when possible.
- Thumbnails decode progressively in the background. The filmstrip scrolls
  horizontally and centers the selected item smoothly. Press `T` to show or
  hide the bar.
- `Delete` moves the current file to the desktop trash when possible. A
  confirmation prompt is optional and off by default.

### Image viewing and adaptive scaling

- Large images fit the available window by shrinking; small images do **not**
  enlarge automatically. `1` returns to 100%, while explicit zoom can range
  from 2% to 10000%.
- `Ctrl` + wheel zooms around the cursor. In Settings, the wheel can instead
  be dedicated to zooming. Otherwise it pans an oversized image and pages
  through files when the image already fits.
- Smooth zoom and a short fade transition keep rapid browsing responsive.
  Neighbour images are preloaded after a brief pause into a 128 MB Qt pixmap
  cache whose keys include absolute path, size and modification time.
- **Auto**, **Smooth (photos)** and **Nearest neighbor (pixel art)** scaling
  modes are available. Static images can receive deferred high-quality
  resampling after zooming stops; animated images and very large images stay
  on the realtime path to bound memory and latency. This is interpolation, not
  AI super-resolution.
- GIF and other animated formats use Qt's animation handler when the installed
  plugin reports animation support.

### Video playback

Qt Multimedia provides playback controls for play/pause, seeking, volume and
mute. Volume and mute are remembered with the application settings. Switching
away from a video stops its player, and playback errors are reported in the
status bar.

### Presentation and customization

Dark and light themes, English and Simplified Chinese labels, a coordinated
toolbar/menu icon system, fullscreen viewing, idle cursor hiding, rotation and
remappable shortcuts are built in. Settings are persisted with `QSettings`.

## Media formats and runtime support

The file list is intentionally based on runtime capability rather than a fixed
extension table. The open-file filter is generated from the same runtime lists.

| Media | Runtime path | Practical boundary |
| --- | --- | --- |
| Images | `QImageReader::supportedImageFormats()` plus `QMovie::supportedFormats()` | Common installations include JPEG, PNG, BMP, GIF, WebP, TIFF, SVG and ICO. The exact list depends on Qt plugins. |
| Modern/extended images | The same Qt plugin discovery and content probing | APNG, AVIF, HEIF/HEIC, JPEG XL and camera RAW (for example DNG/CR2/NEF/ARW) appear only when a compatible Qt image handler is installed. |
| Video containers | Qt Multimedia with the Linux GStreamer backend | MP4, MKV, MOV, AVI and WebM are common examples; a container can still fail when its internal codec is unavailable. |
| Codec diagnostics | Startup checks for selected image handlers and GStreamer decoder elements | A status-bar warning lists missing modern image formats or common H.264/H.265/VP9/AV1 decoders. |

The dependency script installs GStreamer Base, Good, Bad, Ugly and libav
plugins. Installing a standalone codec library does not automatically create a
Qt image handler, and an extension alone is not proof that a file can be
decoded or played.

## Keyboard and mouse reference

Navigation and viewing shortcuts can be changed in **Settings → Shortcuts**.
The table shows the defaults:

| Action | Default |
| --- | --- |
| Previous / next file | `←` / `→`, wheel when the image fits, mouse back/forward buttons |
| First / last file | `Home` / `End` |
| Page backward / forward | `PageUp` / `PageDown` |
| Zoom in / out | `Ctrl++` / `Ctrl+-`, or `Ctrl` + wheel |
| Fit to window / actual size | `F` / `1` |
| Rotate left / right | `Ctrl+L` / `Ctrl+R` |
| Play / pause video | `Space` |
| Adjust video volume | `Ctrl` + wheel |
| Fullscreen | `F11` |
| Show/hide thumbnail bar | `T` |
| Move current file to trash | `Delete` |
| Open file / folder | `Ctrl+O` / `Ctrl+Shift+O` |
| Settings | `Ctrl+,` |

## Build, test and deploy

### Dependencies

- CMake ≥ 3.16 and a C++17 compiler
- Qt 6: Core, Gui, Widgets, Concurrent, Multimedia,
  MultimediaWidgets, LinguistTools and Test
- Qt DBus for the optional Quick Look service
- Qt SVG and image-format runtime plugins
- GStreamer Base, Good, Bad, Ugly and libav runtime plugins

`scripts/install-deps.sh` targets Debian/Ubuntu and is safe to rerun:

```bash
./scripts/install-deps.sh       # interactive
./scripts/install-deps.sh -y    # non-interactive / CI
```

### Build and regression test

```bash
./scripts/build.sh               # Release build in ./build
./scripts/build.sh --debug       # Debug build
./scripts/build.sh --clean       # remove only the selected build directory

ctest --test-dir build --output-on-failure
```

The Qt Test suite covers runtime media recognition, natural sorting, decode
failures, animated GIFs, fit-without-upscaling, interpolation responsiveness,
wheel zoom, thumbnails, folder updates, deletion, toolbar semantics and the
Quick Look D-Bus service when a session bus is available.

GitHub Actions repeats dependency installation, Release compilation, tests,
an offscreen launch smoke test and a staged install-layout check on Ubuntu
22.04, 24.04 and 26.04. The workflow is [.github/workflows/build.yml](.github/workflows/build.yml).

### Install and uninstall

The installer builds a Release target, installs the binary, desktop entry and
icon, and installs the D-Bus service when Quick Look is enabled.

```bash
./install.sh --prefix ~/.local       # per-user install
./install.sh                         # default prefix: /usr/local
./install.sh --deps --prefix ~/.local
./install.sh --uninstall --prefix ~/.local
```

Use the same prefix for uninstall. `--no-previewer` keeps the deployment as a
plain image/video viewer. The desktop entry registers the common image/video
MIME types; runtime discovery remains the source of truth for folder browsing.

### Reproducible documentation screenshots

The capture test uses the application's embedded translations and a generated
sample image, so it does not depend on optional media files. Install a CJK font
(for example `fonts-noto-cjk` on Ubuntu), build the tests, then run:

```bash
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1 QT_FONT_DPI=96 \
FLASHVIEW_SCREENSHOT_DIR="$PWD/screenshots" \
./build/flashview_tests generateDocumentationScreenshots
```

It regenerates eight 1200 × 800 PNGs:
`flashview-{en,zh}-{dark,light}.png` and
`flashview-{en,zh}-{dark,light}-menu.png`. Each README references only its own
interface language and keeps both themes.

## Repository map

```text
src/            Qt viewer, image/video playback, thumbnails, settings and Quick Look service
i18n/           Qt Linguist catalogs (English and Simplified Chinese)
resources/      icon, Qt resource collection and D-Bus service template
screenshots/    language- and theme-specific documentation captures
scripts/        Debian/Ubuntu dependency installer and CMake build wrapper
tests/          Qt Test regression suite and screenshot generator
.github/        GitHub Actions build/test/install-layout workflow
CMakeLists.txt  build graph, optional Quick Look switch and install rules
install.sh      Release build, deployment and uninstall entry point
CHANGELOG.md    user-visible change history
```

## License

No license file is included yet. Until one is added, all rights are reserved
by the author.
