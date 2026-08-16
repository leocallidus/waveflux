# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project follows semantic
versioning where practical.

## [1.3.9-dev]

### Added

- Added Track Fragment Repeat mode (A-B Loop) allowing looped playback of a user-defined section between boundary A and boundary B with forward/reverse loop enforcement and seamless EOS handling.
- Added visual interactive draggable boundary bars (Bar A and Bar B) on the Waveform with shaded loop region highlights, real-time zoom/pan synchronization, and hover removal shortcuts (`Delete` / `Backspace`).
- Added right-click context menu on the Waveform for setting and clearing fragment boundaries, toggling repeat mode, and accessing boundary configuration.
- Added dedicated Fragment Repeat configuration dialog (`FragmentRepeatDialog`) with integrated playback transport controls, position scrubbing slider, boundary steppers, and per-track loop persistence.
- Optimized track search for large playlists (3,000+ tracks) with precomputed search blobs, a dedicated filtered proxy model, cached match maps, and background evaluation that avoids zero-height delegate storms and UI freezes.
- Accelerated playlist metadata ingestion for large libraries with ordered multi-core TagLib workers, small first-result batches, bounded I/O concurrency, $O(1)$ path-to-index lookups, and coalesced UI updates so tags become visible almost immediately.
- Added CMake build speed optimizations including Unity builds (`WAVEFLUX_ENABLE_UNITY_BUILD`), LLD linker support (`-fuse-ld=lld`), compiler `-pipe` flag, and precompiled headers (PCH) for Qt core/GUI headers.
- Added Ogg Vorbis (`.ogg`) as an audio converter output format with capability detection, bitrate selection, and automatic output-extension handling.

### Fixed

- Restored a smooth, capped waveform-generation animation for uncached tracks, including responsive placeholder shimmer, progressive waveform reveal, and correct repaint regions without stale progress from a previous track.
- Fixed custom button implicit sizing and responsive dialog footers so translated action labels remain visible instead of being clipped in Help, Fragment Boundaries, and tag-editing dialogs.
- Improved the Fragment Boundaries mini-player with a clearly visible interactive hover border, a working play/pause action, and concise `A`/`B` boundary controls.
- Replaced legacy stock confirmation controls throughout playlist, converter, equalizer, import, and smart-collection dialogs with the application's Accent-styled buttons.
- Fixed application startup failures related to `TrackFilterProxyModel` QML type/module resolution by exposing dedicated C++-owned filter proxies directly to each playlist view.
- Fixed blank or vertically displaced playlist contents after searching, clearing search text, or switching skins by using dedicated compact/normal proxy models and origin-aware ListView viewport restoration.
- Replaced emoji and font-glyph UI icons with Breeze-compatible themed SVG assets and aligned playback/error and URL dialogs with the application dialog style.
- Stabilized the responsive information sidebar during window resizing by removing recursive layout pressure, keeping the panel instance alive across breakpoints, and preventing album-art redecodes on every resize step.
- Fixed runtime changes to the separate-dialog-windows option by deferring popup reparenting until each open dialog closes, preventing invisible or left-docked windows.
- Fixed audio converter drop-down controls consuming mouse-wheel events so the surrounding converter page continues scrolling normally.

## [1.4.0] - 2026-07-02

### Added

- Added a System setting, disabled by default, to open application modal dialogs as separate top-level windows.
- Centralized QML modal dialogs on a shared `AppDialog` base so the separate-window preference applies consistently across the app.

### Changed

- Raised the minimum Qt requirement to 6.8 because separate-window QML popups rely on `Popup.Window`.

## [1.3.1] - 2026-05-30

### Added

- Added GitHub Releases update checker service (`UpdateChecker`) with manual and automatic background update checks.
- Added global keyboard shortcut management (`ShortcutManager` and `ShortcutRegistry`) with customizable hotkeys.
- Introduced `TrackInfoOverlay`, `VolumeStrip`, `PlaybackAdjustStrip`, and `WaveformHoverTooltip` UI components.
- Added portable ZIP packaging script (`build-portable-zip.ps1`) and WiX 6 MSI installer configuration (`build-wix-installer.ps1`).
- Added automated memory budget check tooling (`check-memory-budgets.ps1`).

### Fixed

- Fixed AppImage dependency scan and runtime library bundling in `build-appimage.sh`.
- Improved Windows SMTC media controls session initialization and track metadata sync.

## [1.3.0] - 2026-05-23

### Added

- Added libopenmpt tracker module playback backend supporting `.mod`, `.xm`, `.s3m`, `.it`, `.669`, `.amf`, and `.stm` files.
- Added single-track (`AudioConverterService`) and batch (`BatchAudioConverterService`) audio conversion with pitch/speed adjustment and preset management.
- Integrated `yt-dlp` import service (`YtDlpImportService`) supporting URL metadata parsing, format selection, and playlist extraction.
- Added playback backend routing abstraction (`PlaybackBackendRouting`) for seamless switching between GStreamer PCM and OpenMPT tracker engines.

### Changed

- Refactored `TrackModel` and `PlaybackController` to support hybrid PCM/tracker track pipelines.

## [1.2.0] - 2026-05-10

### Added

- Added MPRIS desktop integration (`MprisService`) and XDG Portal file picker (`XdgPortalFilePicker`) for Linux environments.
- Added performance profiler module (`PerformanceProfiler`) with overlay display, memory checkpoints, and JSON/CSV export.
- Introduced playlist profiles manager (`PlaylistProfilesManager`) for snapshotting and restoring named playlist states.
- Added CUE sheet parser (`CueSheetParser`) and XSPF playlist parser (`XspfPlaylistParser`).

### Fixed

- Resolved Linux AppImage runtime dependency bundling issues.

## [1.1.0] - 2026-03-15

### Added

- Added distribution packaging scripts for Linux: AppImage (`build-appimage.sh`), Debian (`build-debian-package.sh`), RPM (`build-rpm-package.sh`), and Arch Linux (`build-pacman-package.sh`).
- Added application icon set in SVG/ICO formats and license documentation.

## [1.0.0] - 2026-02-18

### Added

- Initial release of WaveFlux desktop audio player built with C++20, Qt 6.5, Kirigami, GStreamer 1.0, and SQLite.
- Waveform-driven UI scrubbing with cached peak rendering (`WaveformItem`, `WaveformProvider`).
- Local music library search and field-aware smart collections powered by SQLite (`LibraryRepository`, `SmartCollectionsEngine`).
- 10-band audio equalizer with built-in presets (`EqualizerPresetManager`).
- Metadata tag editor supporting ID3v2, FLAC, Vorbis, and MP4 art tags (`TagEditor`).
- Normal and Compact layout modes with customizable Kirigami themes (`ThemeManager`).
