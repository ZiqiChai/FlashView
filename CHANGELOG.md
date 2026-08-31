# Changelog

All notable changes to FlashView are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Auto, Smooth and Nearest Neighbor scaling modes, including lightweight
  pixel-art detection and deferred background resampling for static images.
- Runtime image-format discovery, content/MIME probing and startup warnings for
  missing modern image formats or common video codecs.
- Progressive background thumbnail decoding for responsive large-folder browsing.
- Background neighbour preloading with metadata-aware image cache keys.
- Natural filename sorting, including numeric segments such as `image2` and
  `image10`.
- Live folder and current-file monitoring for additions, removals and replacements.
- Clear empty-folder, unsupported-file and image-decoding error states.
- Remembered video volume and mute state.
- Shortcut conflict detection and a restore-defaults action in Settings.
- Reproducible dark, light and menu documentation screenshots.
- English and Simplified Chinese documentation.
- Qt regression coverage for media recognition, image errors, wheel zoom,
  thumbnails, directory updates and toolbar geometry.

### Changed

- Kept zoom interaction realtime while debouncing high-quality resampling;
  bounded output size and generation checks prevent memory spikes and stale
  background results during rapid zooming or paging.
- Media file discovery now follows the decoders available at runtime instead
  of relying solely on a hard-coded extension list.
- Reworked the toolbar and menu artwork into one consistent rounded-stroke icon
  system.
- Balanced toolbar controls vertically and reduced excessive toolbar height.
- Harmonized menu icon size, row padding and icon-to-text proportions.
- Added distinct Open File and Open Folder actions and icons to the toolbar.
- Made the thumbnail-bar action momentary instead of displaying persistent
  checked state.
- Replaced ambiguous Fit Window, Fullscreen and Thumbnail Bar symbols with
  purpose-specific icons.
- Made fullscreen hide chrome cleanly and restore the previous maximized state.
- Improved the empty-state illustration and HiDPI icon rendering.
- Added `--version`, an embedded application icon and desktop-file association.

### Fixed

- Prevented Fit Window from upscaling low-resolution images above 100%; large
  images are still reduced to fit and explicit zoom can still enlarge them.
- Fixed animated GIF files being displayed as a static first frame.
- Fixed wheel-zoom mode stopping after the image became scrollable.
- Fixed video audio continuing after switching to an image.
- Fixed Space restarting a video instead of pausing and resuming it.
- Fixed stale images remaining visible after a decode failure or empty-folder
  transition.
- Fixed stale pixmaps after an image was replaced in place.
- Fixed corrupt or unreadable files silently leaving inconsistent status data.
- Fixed fullscreen cursor and saved-toolbar state restoration.
- Fixed toolbar buttons being laid out with 10 px above and clipped below the
  toolbar; upper and lower spacing is now symmetric.
- Fixed toolbar tooltips not updating after shortcut changes.
- Fixed dependency installation omitting the Qt SVG and extended image-format
  plugins required by documented media formats.
