# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project follows semantic
versioning where practical.

## [1.3.9-dev] 2026-07-02 - 2026-08-17

### Added

- Implemented comprehensive Playlist Column Customization system (`PlaylistColumnLayoutManager`) per specification:
  - **Catalog of 24 Column Types**: Added full support for metadata columns (`playlistPosition`, `trackSummary`, `title`, `artist`, `album`, `duration`, `bitrate`, `trackNumber`, `year`, `genre`, `description`, `composer`, `originalArtist`, `copyright`, `url`, `encoder`) and technical audio columns (`format`, `sampleRate`, `bitDepth`, `bpm`, `channelCount`, `fileName`, `filePath`, `dateAdded`).
  - **Independent Skin Layouts**: Independent column visibility, arrangement, and default layouts for Standard (Normal) and Compact skins, persisted in `QSettings`.
  - **Three Visibility Modes**: Configurable `Shown` (always visible), `Automatic` (responsive display based on discrete viewport width breakpoints), and `Hidden` modes per column.
  - **Playlist Columns Configuration Dialog (`PlaylistColumnsDialog`)**:
    - Interactive dialog accessible via menu bar, playlist toolbar, and table header context menus.
    - Skin tabs switcher with cross-skin layout copying (`Copy from Standard/Compact`) and skin reset actions.
    - Compact skin header display modes (`Automatic`, `Always Shown`, `Always Hidden`).
    - Column reordering (Move Up / Move Down) and visibility combo selectors with responsive wrapping.
    - All-hidden warning banner with one-click restoration of default columns.
    - Full Breeze SVG themed icons, dark/light theme integration, and complete Russian/English localization.
  - **Interactive Header Context Menu**: Right-click context menu on any playlist column header for quick column toggling, column reset, and direct access to the configuration dialog.
  - **Metadata Pipeline & Security**: Extended `TrackModel` with TagLib extraction for composer, original artist, copyright, and encoder, plus secure URL validation (`isUrlSchemeAllowed`) with clickable links and copy actions.
  - **Hover Tooltips**: Integrated automatic hover tooltips for truncated column cells to inspect full text content without modifying column width.
  - **High-Performance Rendering**: Implemented discrete width bucketing and SceneGraph caching in `PlaylistColumnLayoutManager` alongside lazy context menu loaders and optimized cell layouts to ensure smooth 60 FPS window resizing and zero column-switching lag.
  - **Unit Test Coverage**: Added comprehensive test suite `tst_PlaylistColumnLayoutManager` covering catalog completeness, formatting, persistence, sanitization, copy/reset, responsive bucketing, and visibility toggling.

- Added comprehensive track chapter support and visualization for files containing embedded chapters (ID3v2 `CHAP`/`CTOC` frames, MP4/M4A chapters, and Vorbis comments):
  - **InfoSidebar**: Added an interactive "CHAPTERS" list displaying all chapter names, start times, and durations with active chapter tracking/highlighting and click-to-seek functionality.
  - **WaveformView & CompactSkin**: Added visual chapter overlays featuring boundary marker lines, notch ticks, active segment tinting, and chapter title labels on the waveform.
  - **Waveform Hover Tooltip & Overlays**: Enhanced waveform hover preview and track info templates (`%h` placeholder) to display the chapter name at the cursor or playback position.
  - **Playback Transport & Navigation**: Added "Previous Chapter" (`Alt+PageUp`) and "Next Chapter" (`Alt+PageDown`) navigation actions in Main playback menus, HeaderBar, ControlBar, and Waveform context menus.
  - **ControlBar**: Added real-time chapter indicator badge with chapter popup navigation menu.
  - **Playlist Table**: Added `CHAP` indicator badge for chapter-bearing tracks and a dedicated "Chapters" right-click context submenu to instantly jump into any chapter of a playlist track.

- Implemented centralized UI metrics and design token system (`UiMetrics`) available across C++ and QML, exposing semantic typography roles (`micro`, `caption`, `body`, `bodyStrong`, `subtitle`, `title`, `display`), proportional spacing tokens (`spaceXXS` through `spaceXXL`), standard control/icon heights, and font-aware responsive breakpoints.
- Added font-metrics-aware scaling engine in `ThemeManager` deriving `fontMetricsScale` and `playlistFontMetricsScale` from real line-spacing ratios against baseline system fonts rather than naive point size multipliers.
- Added support for independent playlist font family selection (`playlistFontFamily`) and dedicated playlist line-height/scale metrics calculation.
- Added real-time application font updates across all UI views and dialogs without requiring application restarts.
- Added automated unit test suite `tst_ThemeManagerUiMetrics` and static `font.pixelSize` regression audit in `tst_AppDialog` ensuring zero unmigrated pixel sizes across the codebase.
- Added Track Fragment Repeat mode (A-B Loop) allowing looped playback of a user-defined section between boundary A and boundary B with forward/reverse loop enforcement and seamless EOS handling.
- Added visual interactive draggable boundary bars (Bar A and Bar B) on the Waveform with shaded loop region highlights, real-time zoom/pan synchronization, and hover removal shortcuts (`Delete` / `Backspace`).
- Added right-click context menu on the Waveform for setting and clearing fragment boundaries, toggling repeat mode, and accessing boundary configuration.
- Added dedicated Fragment Repeat configuration dialog (`FragmentRepeatDialog`) with integrated playback transport controls, position scrubbing slider, boundary steppers, and per-track loop persistence.
- Optimized track search for large playlists (3,000+ tracks) with precomputed search blobs, a dedicated filtered proxy model, cached match maps, and background evaluation that avoids zero-height delegate storms and UI freezes.
- Accelerated playlist metadata ingestion for large libraries with ordered multi-core TagLib workers, small first-result batches, bounded I/O concurrency, $O(1)$ path-to-index lookups, and coalesced UI updates so tags become visible almost immediately.
- Added CMake build speed optimizations including Unity builds (`WAVEFLUX_ENABLE_UNITY_BUILD`), LLD linker support (`-fuse-ld=lld`), compiler `-pipe` flag, and precompiled headers (PCH) for Qt core/GUI headers.
- Added Reset Playlist action across all skins (Normal skin File & Library menus, HeaderBar popup menu, CompactSkin hamburger menu, PlaylistView sort & context menus, and PlaylistTable context menus) with configurable `Ctrl+Alt+R` shortcut and localized strings (EN/RU).
- Implemented baseline playlist snapshot tracking and restoration in `TrackModel` (`resetPlaylist()`, `canResetPlaylist`, `captureBaselineSnapshot()`, `exportBaselineSnapshot()`) to seamlessly revert user reordering, custom drag-and-drop changes, column sorting, and deleted tracks back to initial ingestion/addition order while preserving currently playing track continuity without interrupting audio playback.
- Added Breeze-compatible themed reset/revert SVG icons (`document-revert-dark.svg` and `document-revert-light.svg`).
- Implemented unit test coverage in `tst_TrackModel` verifying playlist order restoration after drag-and-drop moves, sorting by name/date/duration/bitrate, track deletions, incremental additions, snapshot restorations, and active playing track index tracking.
- Added Reset Playlist toolbar button in `PlaylistView` next to Shuffle and Sort.
- Added visible Current Playlist entry in `CollectionsSidebar` showing real-time track count, active status highlight, and one-click "Save as playlist" action.
- Added automatic named playlist profile generation upon adding folders to an empty working playlist, immediately creating and selecting a persistent, autosaved playlist named after the added folder.
- Added Ogg Vorbis (`.ogg`) as an audio converter output format with capability detection, bitrate selection, and automatic output-extension handling.
- Added a System setting, disabled by default, to open application modal dialogs as separate top-level windows.
- Centralized QML modal dialogs on a shared `AppDialog` base so the separate-window preference applies consistently across the app.

### Changed

- Raised the minimum Qt requirement to 6.8 because separate-window QML popups rely on `Popup.Window`.
- Migrated 100% of QML files to semantic point sizes (`font.pointSize: UiMetrics.*PointSize`) and metric tokens, completely eliminating hardcoded `font.pixelSize` and deprecated `fontSizeMultiplier` throughout the entire application.
- Refactored responsive layout thresholds across normal and compact skins (`Main.qml`, `ControlBar.qml`, `WaveformView.qml`, and all dialogs) to use font-aware dynamic breakpoints (`UiMetrics.breakpoint(...)`), preventing clipping or overlapping at large font sizes (8–24 pt).
- Standardized all modal and standalone dialogs on bounded dynamic scaling (`boundedDialogSize` / `fitDialogSize`) with `ScrollView` content wrapping to ensure accessibility on small viewports and high font scales.

### Fixed

- Fixed false-positive chapter detection on tracks without chapters by strictly validating Vorbis chapter tag keys and timestamps, and added a user setting (`settings.showPlaylistChapterBadge`) to toggle the `CHAP` badge in the playlist table.
- Fixed vertical scrollbar overlapping track duration timestamps in the right-hand InfoSidebar chapters section by adjusting delegate margins to accommodate the scrollbar.
- Fixed empty chapters submenu when right-clicking tracks in playlist table and playlist view by dynamically populating chapter menu items with accurate start timestamps and titles upon menu activation.
- Fixed a critical regression in Playlist Profile editing where clicking "Save changes" in the Edit Playlist dialog would be overwritten upon reloading the active profile due to an unconditional stale in-memory autosave flush.
- Fixed search input text clipping and horizontal truncation in HeaderBar, PlaylistView, and CompactSkin across all font sizes and zoom levels by clearing restrictive internal padding, aligning vertically to center, and ensuring proper font metrics and container dimensions.
- Fixed font family and font size reactivity in drop-down lists (`AccentComboBox`, `SettingComboRow`), buttons, switches, checkboxes, radio buttons, and menus when changing font settings in preferences.
- Fixed QML module singleton type registration for `UiMetrics` in CMake, eliminating startup `undefined` property evaluations and associated layout calculation freezes.
- Resolved ToolButton dimension binding loops in `VolumeStrip` and `PlaybackAdjustStrip`.
- Added missing Breeze-compatible themed SVG icons (`media-playlist-consecutive-dark.svg` and `media-playlist-consecutive-light.svg`).
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
