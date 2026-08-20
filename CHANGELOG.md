# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project follows semantic
versioning where practical.

## [1.4.0] 2026-08-20

### Added

- Track change notifications (`DesktopNotificationService`) on Linux (via DBus `org.freedesktop.Notifications.Notify`) and Windows (via `QSystemTrayIcon::showMessage`). Displays track title/artist/filename, album, duration, cover art (`image-path`), and the `waveflux` icon, with debouncing for rapid track switches.
- `playback.notifyOnTrackChange` setting in `AppSettingsManager` (enabled by default, stored in `QSettings`), configurable under *Audio Presentation* in `PlaybackSettingsPage.qml` with search indexing and reset support.
- English and Russian translations for track change notification settings and labels.
- Dedicated unit tests in `tests/tst_DesktopNotificationService.cpp`, verified in `tests/tst_AppSettingsManager.cpp` and `tests/tst_SettingsRegistry.cpp`.
- Redesigned URL import dialog (`YtDlpImportDialog.qml`) with a four-tab layout: *Queue & Sources*, *Active Downloads*, *Format & Settings*, and *Report & History*.
- Custom yt-dlp CLI arguments support: probe flags (`probeCustomArgs`) for metadata inspection and download flags (`downloadCustomArgs`) for audio extraction, with quote-aware tokenization and instant typing updates.
- Post-processing options for URL imports: metadata embedding (`--embed-metadata`), thumbnail embedding (`--embed-thumbnail`), 1:1 square cover cropping via FFmpeg postprocessor arguments, and metadata stripping (`--no-write-comments`, TagLib pass for comments and download URLs).
- Optional `aria2c` multi-connection downloader integration (`--downloader aria2c`) with configurable connections (1 to 16, default 16) and minimum split size (1 to 100 MiB, default 20 MiB).
- Persistent storage for URL import preferences in `AppSettingsManager` across restarts.
- Fallback icon aliases in `IconResolver.js` for missing SVG icons across themes (`download`, `network-workgroup`, `system-search`, `view-refresh`, `dialog-ok`, `folder`, `view-hidden`, `document-open-recent`, `transform-crop-and-resize`).
- Unit tests in `tests/tst_YtDlpImportService.cpp` covering post-processing options, CLI argument generation, and preset persistence.
- Extended metadata editing in single (`TagEditorDialog.qml`) and batch (`BulkTagEditorDialog.qml`) tag editors: Genre, Comment, Composer, Original Artist, Copyright, URL, Encoder, and BPM via ID3v2 frames and Vorbis comments.
- Cover image export (`exportCoverImage`, `suggestedCoverFileName`) to disk via native file dialogs, with support for UTF-8 and percent-encoded non-Latin paths.
- Chapter marker editor configured in seconds, writing to ID3v2 `CHAP`/`CTOC` frames and Vorbis `CHAPTERxxx` tags.
- Technical audio info tab in `TagEditorDialog` displaying format, bitrate, sample rate, channels, file size, duration, and path.
- Warning banner when editing tracker modules (MOD, XM, S3M, IT) where standard tags are not supported.
- Multi-track selection in playlist tables using `Ctrl` and `Shift`, with proxy index mapping during search and sorting, and fast selection lookups.
- Tag editor unit tests in `tests/tst_TagEditor.cpp`.
- Four-tab layouts for single (`AudioConverterDialog.qml`) and batch (`BatchAudioConverterDialog.qml`) audio converters: Format & Quality / Format & Output, Trim & DSP / DSP & Enhancements, Live Simulation, and Source Info / Queue & Logs.
- Built-in DSP effects in audio conversion: speed (0.25x-3.00x), tempo (0.50x-3.00x), pitch shifting (-10.0 to +10.0 semitones), echo (0%-100%), reverb (0%-100%), chorus (0%-100%), flanger (0%-100%), bass shelf (0.00x-2.00x), stereo width (1.00x-5.00x), center-channel voice suppression, and EQ bake-in.
- Single-track converter trim controls configured in seconds (`00:00`).
- Batch converter queue management: toolbar actions (*Add Files*, *Add Folder*, *Remove Selected*, *Clear Queue*), status filter chips (*All*, *Pending*, *Succeeded*, *Failed*), and per-item retry/removal.
- Real-time format and quality simulation in audio converter preview (`WaveFlux::Dsp::FormatQualitySimulator`), modeling low-pass cutoffs by bitrate, quantization noise for lossy codecs (MP3, OGG, AAC, Opus), sample rate decimation (8 kHz to 32 kHz), mono downmixing, and 10-band EQ without restarting playback.
- Sample fragment looping (`previewLoop`) and scrubber slider (`previewSeekSlider`) in the audio converter preview player.
- Right-click and long-press context menu to reset individual parameters to default in `AudioConverterDialog` (`AudioConverterService::resetParameter`).
- "Refresh Playlist" action (`F5`, `file.refreshPlaylist`) in standard and compact skins to rescan folders, pick up new or removed files, refresh metadata, and maintain the current playback position.
- Dedicated preview player in `AudioConverterDialog` with configurable start/end boundaries (`previewStartMs`, `previewEndMs`), accurate GStreamer flush seeking, and auto-pausing of main playback during preview.
- OGG Vorbis format option for audio extraction and postprocessing in `YtDlpImportService`, `AppSettingsManager`, and `YtDlpImportDialog`.
- Redesigned Settings dialog (`SettingsDialog.qml`) with two-pane navigation, instant search with match highlighting, and 9 category pages (`qml/settings/`): General, Appearance, Playlist, Playback, Waveform, Track Info, System & Tools, Shortcuts, and Advanced & Reset.
- Centralized settings registry (`SettingsRegistry`) defining setting metadata, keywords, control types, dependencies, and reset scopes.
- Reusable settings UI components in `qml/components/` for toggle switches, sliders, combo boxes, color pickers, file paths, and action buttons.
- Keyboard shortcut editor with live key capture (`shortcutCaptureDialog`), conflict detection (`shortcutConflictDialog`), and per-action reset.
- Unit tests in `tst_SettingsRegistry` and `tst_AppSettingsManager`.
- Playlist column customization manager (`PlaylistColumnLayoutManager`) with 24 metadata and technical audio columns.
- Independent column layouts, ordering, and visibility modes (Shown, Automatic, Hidden) for standard and compact player skins.
- Playlist columns configuration dialog (`PlaylistColumnsDialog`) with column reordering, layout copying between skins, and default restoration.
- Header context menu for quick column toggling and reset.
- Hover tooltips for truncated text in playlist cells.
- Unit tests in `tst_PlaylistColumnLayoutManager`.
- Embedded chapter support (ID3v2 `CHAP`/`CTOC`, MP4/M4A, and Vorbis comments) with seeking from the InfoSidebar chapter list.
- Visual chapter markers, notch ticks, and title labels on waveforms (`WaveformView` and `CompactSkin`).
- Chapter navigation shortcuts: "Previous Chapter" (`Alt+PageUp`) and "Next Chapter" (`Alt+PageDown`).
- Chapter indicator badge and navigation popup in the control bar, plus a `CHAP` badge and chapter jump menu in the playlist table.
- Centralized UI metrics system (`UiMetrics`) exposing semantic typography roles, spacing tokens, standard control heights, and responsive breakpoints.
- Line-spacing-based font scaling engine in `ThemeManager` with support for an independent playlist font family (`playlistFontFamily`).
- Dynamic application font updates across views and dialogs without restarting.
- Unit tests in `tst_ThemeManagerUiMetrics` and font audit tests in `tst_AppDialog`.
- A-B fragment loop playback between user-defined start and end boundaries, with forward and reverse playback support.
- Interactive draggable boundary markers on the waveform with region highlighting, zoom synchronization, and removal via `Delete` or `Backspace`.
- Waveform context menu for setting and clearing loop points.
- Fragment repeat configuration dialog (`FragmentRepeatDialog`) with playback controls, scrub slider, and per-track loop persistence.
- Search indexing optimization for large playlists (3,000+ tracks) using precomputed search blobs, cached match maps, and background evaluation.
- Multi-threaded TagLib metadata loading with bounded I/O concurrency and batched UI updates for large music libraries.
- Build performance options in CMake: Unity builds (`WAVEFLUX_ENABLE_UNITY_BUILD`), LLD linker support (`-fuse-ld=lld`), compiler `-pipe` flag, and precompiled headers for Qt headers.
- "Reset Playlist" action (`Ctrl+Alt+R`) in standard and compact skins to revert user reordering, sorting, and track deletions back to the initial folder order without interrupting playback.
- Snapshot tracking and restoration in `TrackModel` (`resetPlaylist()`, `canResetPlaylist`), verified in `tst_TrackModel`.
- Active playlist item in `CollectionsSidebar` with track count and "Save as playlist" action.
- Automatic playlist profile creation when adding a folder to an empty playlist.
- OGG Vorbis (`.ogg`) output format support in the audio converter.
- Option in System settings to open modal dialogs as separate top-level windows instead of in-window overlays.
- Migrated modal dialogs to a shared `AppDialog` component.
- Redesigned DSP Manager dialog (`DspManagerDialog.qml`) with five tabs: General, EQ, Volume, Mix, and Silence Removal.
- Audio effects in the General tab: Echo, Chorus, Speed (0.25-3.00x), Reverb, Bass shelf, Tempo (0.50-3.00x), Flanger, Stereo Width (1.00-5.00x), Pitch shifting (-10.0 to +10.0 semitones), center-channel voice suppression, and pause/track fade transitions.
- 10-band graphic equalizer with preset management (create, rename, delete, import, export) in the EQ tab.
- Volume and dynamics controls: logarithmic volume curves, loudness compensation, peak amplitude normalization, and ReplayGain (Track/Album modes with preamp adjustment).
- Track transition controls: manual and automatic crossfades, fade-in, and fade-out durations in the Mix tab.
- Silence removal filter with configurable duration threshold (50-5000 ms) and noise floor (-90 to -20 dBFS).
- Persistent DSP settings manager (`DspSettingsManager`) with preset import/export and parameter reset scopes.
- Real-time PCM processing pipeline (`DspProcessor`) hooked into GStreamer and OpenMPT audio backends.
- Unit tests in `tst_DspSettingsManager`, `tst_DspProcessor`, and `tst_SilenceRemoval`.

### Changed

- Raised the minimum Qt requirement to 6.8 for `Popup.Window` support in separate-window dialogs.
- Switched QML text elements to semantic point sizes (`UiMetrics.*PointSize`) and metric tokens, replacing fixed pixel sizes and `fontSizeMultiplier`.
- Updated responsive layout breakpoints across normal and compact skins (`Main.qml`, `ControlBar.qml`, `WaveformView.qml`, and dialogs) using dynamic font-aware breakpoints (`UiMetrics.breakpoint`).
- Standardized dialog sizing (`boundedDialogSize` / `fitDialogSize`) with `ScrollView` wrappers to prevent content overflow at larger font sizes or smaller screen resolutions.

### Fixed

- Fixed dialog title text overlapping content in Reset Confirmation (`resetConfirmDialog`), Factory Reset (`factoryResetDialog`), Shortcut Capture (`shortcutCaptureDialog`), and Shortcut Conflict (`shortcutConflictDialog`) dialogs by removing unstyled default Qt Quick dialog headers.
- Fixed duplicate headers in `DspManagerDialog` by disabling the default Qt Quick title bar (`header: null`).
- Fixed DSP Manager parameters not affecting playback by instantiating `DspSettingsManager` before `AudioEngine`, routing speed/tempo/tonality to the playback transport, and applying DSP filters via a GStreamer pad probe on a stable identity element.
- Fixed waveform playhead seeking and marker drift when adjusting Speed or Tempo in DSP Manager: rate and tempo now compose multiplicatively into the pipeline segment rate via `GST_SEEK_FLAG_INSTANT_RATE_CHANGE` rather than modifying SoundTouch stream time ratios.
- Fixed inverted vertical slider behavior in `AccentSlider` by aligning handle and fill geometry with Qt's vertical `position` mapping.
- Fixed DSP parameter sliders ignoring mouse drag input by replacing conflicting `TapHandler`s with a right-click and long-press `MouseArea`.
- Fixed clipped EQ preset lists in `DspManagerDialog` by replacing the stacked layout with an auto-sizing `Flickable`/`Repeater` list.
- Fixed runtime warnings (`Unable to assign [undefined] to bool`) in `DspEqualizerPage` when initialized without an engine or preset manager.
- Fixed MPRIS track duration staying stuck after changing playback rate by refreshing `durationChanged` in `AudioEngine::setPlaybackRate` and updating `mpris:length` in `MprisService`.
- Fixed blank Playback settings page caused by unhandled JavaScript capability lookups and missing `matchesSearch` visibility bindings in `SettingRow.qml`.
- Fixed Settings dialog search bar overlapping the window header by suppressing default Qt Quick dialog title bars.
- Fixed reactive language switching in Settings dialog by binding pages and setting rows to `appSettings.translationRevision`.
- Fixed untranslated localization keys in English and Russian catalogs (`dialogs.close`, `menu.settings`, `menu.tools`, `settings.closeToTray`, `settings.minimizeToTray`, `settings.startMinimizedToTray`, `settings.compactPlaylistTrackNumberVisible`, `settings.confirmTrash`).
- Fixed layout overflow and misaligned action buttons in Keyboard Shortcuts settings page when using localized text.
- Fixed false-positive chapter detection on tracks without chapters by strictly validating Vorbis chapter tag keys and timestamps, and added a setting (`settings.showPlaylistChapterBadge`) to toggle the `CHAP` badge in the playlist table.
- Fixed vertical scrollbar overlapping track duration timestamps in the InfoSidebar chapters list by adding margin clearance.
- Fixed empty chapters submenu when right-clicking tracks in the playlist table by populating chapter items on menu open.
- Fixed a bug where saving changes in the Edit Playlist dialog could be overwritten by a stale autosave flush on active profile reload.
- Fixed search input text clipping in HeaderBar, PlaylistView, and CompactSkin across large font sizes.
- Fixed font family and size updates not applying dynamically to drop-down lists (`AccentComboBox`, `SettingComboRow`), buttons, switches, checkboxes, radio buttons, and menus.
- Fixed `UiMetrics` singleton registration in CMake, resolving startup evaluation errors.
- Resolved ToolButton dimension binding loops in `VolumeStrip` and `PlaybackAdjustStrip`.
- Added missing themed SVG icons: `media-playlist-consecutive-dark.svg` and `media-playlist-consecutive-light.svg`.
- Restored waveform generation shimmer animation for uncached tracks without stale progress from previously loaded tracks.
- Fixed button sizing and responsive dialog footers clipping localized labels in Help, Fragment Boundaries, and tag-editing dialogs.
- Fixed Fragment Boundaries mini-player hover border, play/pause action, and `A`/`B` boundary controls.
- Replaced default stock confirmation buttons with Accent buttons across playlist, converter, equalizer, import, and smart-collection dialogs.
- Fixed startup failure related to `TrackFilterProxyModel` QML type resolution by exposing C++ filter proxies directly to each playlist view.
- Fixed blank or displaced playlist views after searching or switching skins by restoring the viewport origin on dedicated proxy models.
- Replaced font-glyph and emoji icons with Breeze SVG assets across playback, error, and URL dialogs.
- Fixed responsive InfoSidebar layout jitter and prevented redundant album art redecodes during window resizing.
- Fixed runtime switching of the separate-dialog-windows setting by deferring popup reparenting until open dialogs close.
- Fixed audio converter drop-down controls intercepting mouse wheel scroll events from the parent page.

## [1.3.1] - 2026-05-30

### Added

- GitHub Releases update checker service (`UpdateChecker`) with manual and background checks.
- Global keyboard shortcut manager (`ShortcutManager`, `ShortcutRegistry`) with customizable hotkeys.
- UI components: `TrackInfoOverlay`, `VolumeStrip`, `PlaybackAdjustStrip`, and `WaveformHoverTooltip`.
- Packaging scripts for portable ZIP (`build-portable-zip.ps1`) and WiX 6 MSI installer (`build-wix-installer.ps1`).
- Memory budget validation script (`check-memory-budgets.ps1`).

### Fixed

- Fixed AppImage dependency scan and runtime library bundling in `build-appimage.sh`.
- Fixed Windows SMTC media controls session initialization and metadata synchronization.

## [1.3.0] - 2026-05-23

### Added

- libopenmpt tracker module playback backend supporting `.mod`, `.xm`, `.s3m`, `.it`, `.669`, `.amf`, and `.stm` files.
- Single-track (`AudioConverterService`) and batch (`BatchAudioConverterService`) audio conversion with pitch/speed adjustment and preset management.
- `yt-dlp` import service (`YtDlpImportService`) with URL metadata extraction, format selection, and playlist downloading.
- Playback backend router (`PlaybackBackendRouting`) for switching between GStreamer PCM and OpenMPT tracker engines.

### Changed

- Refactored `TrackModel` and `PlaybackController` to support hybrid PCM and tracker playback pipelines.

## [1.2.0] - 2026-05-10

### Added

- MPRIS desktop integration (`MprisService`) and XDG Portal file picker (`XdgPortalFilePicker`) on Linux.
- Performance profiler (`PerformanceProfiler`) with overlay display, memory checkpoints, and JSON/CSV export.
- Playlist profiles manager (`PlaylistProfilesManager`) for saving and restoring named playlist snapshots.
- CUE sheet (`CueSheetParser`) and XSPF playlist (`XspfPlaylistParser`) parsers.

### Fixed

- Fixed Linux AppImage runtime dependency bundling issues.

## [1.1.0] - 2026-03-15

### Added

- Linux packaging scripts for AppImage (`build-appimage.sh`), Debian (`build-debian-package.sh`), RPM (`build-rpm-package.sh`), and Arch Linux (`build-pacman-package.sh`).
- Application icon set in SVG/ICO formats and license documentation.

## [1.0.0] - 2026-02-18

### Added

- Initial release of WaveFlux desktop audio player built with C++20, Qt 6.5, Kirigami, GStreamer 1.0, and SQLite.
- Waveform-driven playback scrubbing with cached peak rendering (`WaveformItem`, `WaveformProvider`).
- Local music library search and smart collections backed by SQLite (`LibraryRepository`, `SmartCollectionsEngine`).
- 10-band audio equalizer with preset management (`EqualizerPresetManager`).
- Tag editor supporting ID3v2, FLAC, Vorbis, and MP4 tags (`TagEditor`).
- Standard and Compact layout modes with customizable themes (`ThemeManager`).
