# Changelog

All notable changes to this project are documented in this file.

The format is based on Keep a Changelog, and this project follows semantic
versioning where practical.

## [1.4.1] 2026-09-04

### Added

- Native Lyrics Panel and synchronizer implementation:
  - **LRC Parser & Decoder (`src/lyrics/LrcParser.*`)**: High-performance parser with UTF-8/UTF-16 auto-detection, multi-timestamp cues consolidation, word-tag stripping (`<mm:ss.xx>`), instrumental break detection from blank cues, metadata parsing (`[offset:]`, `[ti:]`, `[ar:]`, `[al:]`), and bounded memory/length protection.
  - **Fuzzy Matcher & Scorer (`src/lyrics/LyricsMatcher.*`)**: Diacritic removal, punctuation stripping, edition tag cleaner, Levenshtein distance, duration tolerance filtering (<=2s synced, <=5s plain), and weighted ranking.
  - **Lyrics Cache Database (`src/lyrics/LyricsCache.*`)**: Dedicated SQLite database (`lyrics_cache.db`) with 30-day positive caching TTL, 6-hour negative hit caching, track delay and manual candidate override persistence, and bounded LRU eviction (50 MiB / 1000 documents).
  - **Online & Local Providers (`src/lyrics/`)**:
    - Local sidecar provider searching for `<audio-base>.lrc`, `<audio-base>.trackNN.lrc` (for CUE sheets), and `.txt` files.
    - LRCLIB provider (`GET /api/get` + search fallback with rate limiting, custom User-Agent, and 429 backoff).
    - Lyrics.ovh provider for plain text lyrics fallback.
    - Manual lyrics import/export (.lrc and .txt) and system clipboard integration.
  - **Lyrics Playback Controller & Line Model (`src/lyrics/LyricsController.*`, `src/lyrics/LyricsLineModel.*`)**: Real-time binary search synchronization tracking `PlaybackController::activeTrackIndex` and position, supporting CUE track offset subtraction (`songPositionMs = posMs - track.cueStartMs`), A-B loop seek bounds clamping, user delay adjustments (-/+100ms stepper, reset), and smooth auto-follow with manual drag pause and floating resume button.
  - **UI Views & Dialogs (`qml/LyricsPanel.qml`, `qml/LyricsSearchDialog.qml`)**:
    - Resizable sidebar panel docked next to the playlist table with splitter handle and collapsible state.
    - Status badge indicators for mode (`LRC`, `TXT`, `INST`) and provenance (`Sidecar`, `Imported`, `LRCLIB`, `Lyrics.ovh`).
    - Dedicated search and candidate picker dialog with query inputs, live search, metadata and duration badges, preview pane, and candidate selection.
    - View menu integration, `Ctrl+Alt+L` toggle shortcut, and settings under *Playback Settings* for privacy opt-in (`lyricsOnlineEnabled`), auto-lookup, preferred format, auto-follow, and font scaling.
  - **Test Coverage**: Added test suites `tst_lrc_parser`, `tst_lyrics_matcher`, `tst_lyrics_cache`, `tst_lyrics_playback_sync`, `tst_lyrics_providers`, and `tst_lyrics_controller`. The full project suite passes all 39 test suites, including QML startup verification.
- Added persistent lyrics presentation options:
  - Open the full Lyrics panel from both View menus, the standard-skin toolbar, the compact-skin menu, or **Settings → Playback → Lyrics**.
  - Detach lyrics into a separate, resizable window using a toggle in the panel menu or Lyrics settings. Closing the window hides lyrics without stopping playback; compact mode automatically uses a separate window without changing the saved docking preference.
  - Show a compact Lyrics block in the info sidebar with plain text or clickable, timestamped synchronized lines, automatic following, and search/refresh buttons. The block shares the existing lyrics controller, is enabled by default, and can be disabled independently in Lyrics settings.
  - Find the panel visibility, separate-window, and info-sidebar options through settings search.
- Added reusable `AccentTextField` controls with theme-aware text, placeholder, selection, and keyboard-focus styling.
- Added deterministic offline regression coverage for provider fallbacks, malformed responses, rate limits, cancellation, deadlines, payload limits, edited search metadata, privacy, cache retries, stale replies, and imports. Extended UI tests cover light/dark palettes, narrow/wide layouts, both dialog modes, detached-window lifecycle, info-block interactions, and persisted presentation preferences.
- **Reverse Playback in Audio Converters**:
  - Added the ability to enable reverse playback across all audio converter workflows:
    - **Single Track Audio Converter (`AudioConverterDialog.qml`)**: Toggle reverse playback under the Transform card to render and export the converted output backwards in reverse.
    - **Batch Audio Converter (`BatchAudioConverterDialog.qml`)**: Convert entire queues or playlist selections in reverse with support for user presets, draft restoration, and job reporting.
    - **Reverse Audio Preview**: In-dialog preview plays the audio fragment in reverse with synchronized bidirectional progress mapping (`0.0` at end, `1.0` at start) and looping.
    - **Seamless DSP & Trim Integration**: Works smoothly alongside fragment trimming, speed, tempo, pitch shifting, graphic equalizer, and reverb through frame-accurate chunked intermediate PCM reversal.
    - Added full bilingual localization (EN / RU) for reverse playback settings, summaries, and dialog descriptions.

### Changed

- Polished the Lyrics panel and candidate picker to use the application's existing theme colors, typography, spacing, and SVG icons consistently.
- Made the search dialog window-centered and scrollable, with labeled search fields, responsive stacked layouts on narrow windows, readable candidate previews, keyboard navigation, and explicit Apply actions.
- Moved track details and synchronization-offset controls out of the crowded panel toolbar; added accessible action names and visible import/export failure messages.
- Added English and Russian guidance for lyrics searches, previews, retries, file errors, and presentation settings.
- Manual lyrics searches now rank against the edited title, artist, and optional album instead of the original track metadata, omit the original duration constraint, and require explicit selection. Ambiguous automatic matches no longer interrupt playback with a popup.
- Improved LRCLIB lookup by falling back from exact matching to search and then normalized, edition-free metadata. Useful primary-provider results can complete without waiting for the secondary provider, while respecting the plain/synchronized preference.
- Hardened lyrics requests with transfer timeouts, absolute deadlines, response-size limits, cancellation-safe cleanup, and numeric `Retry-After` cooldown handling. Imports reject oversized files, and exports use atomic file replacement.

### Fixed

- Fixed nonexistent theme-property references that caused black/white or inconsistently styled lyrics controls, and missing candidate text that left previews empty.
- Fixed lyrics lookup reading the wrong track-duration field; changes to active-track metadata now refresh the lookup.
- Fixed Unicode normalization removing non-Latin letters, and improved matching of remastered-edition title suffixes.
- Fixed Lyrics.ovh results using the original track metadata after a manual search instead of the edited query.
- Fixed network and parse failures being treated as clean misses and negative-cached. Retry now bypasses cached query results and the automatic-lookup setting without bypassing online consent.
- Fixed stale lyrics replies surviving track changes, track removal, privacy changes, or successful imports; fixed reentrant reply cancellation and cleanup.
- Fixed imports being superseded by an older manual selection on subsequent lookup, and corrected cached-provider attribution.
- Fixed completed-line highlighting across blank verse separators, consecutive instrumental-break cues, forward/backward seeks, and track completion. The line model now retains playback progress independently of the highlighted line and refreshes every affected row.
- Fixed missing Lyrics entries in the direct View menu and compact-skin menu, and ensured detached lyrics do not hide or reserve space in the docked playlist layout.
- Fixed severe startup delays caused by redundant disk metadata scans on session restore and track insertion: files with technical audio metadata and duration already loaded from session or cache are no longer misidentified as missing metadata when optional album tags are empty.
- Fixed distorted and chaotic playlist column layouts (`PlaylistTable` and `CompactSkin`) where responsive width buckets were erroneously supplied instead of available pixel widths to `PlaylistColumnLayoutManager::effectiveVisibleColumns`.
- Fixed `UiMetrics` singleton resolution in `HeaderBar`, `PlaylistTable`, and `Main` by explicitly importing module `WaveFlux`, preventing uninstantiated component fallback that collapsed header dimensions and enlarged SVG logo rendering.
- Fixed GUI thread lockup on large playlists during playback start and track location by replacing synchronous unbounded `contentY` assignments with virtualized `ListView::positionViewAtIndex(proxyIndex, ListView.Center)`.
- Fixed recursive layout thrashing in `PlaylistTable` by removing viewport sync triggers from `onContentHeightChanged` and `onHeightChanged`.
- Fixed `QString::arg` missing argument warnings for `ytDlpImport.summaryTags`, `ytDlpImport.summaryTagsDetails`, `equalizer.deletePresetConfirmTitle`, and `equalizer.deletePresetConfirmMessage` in English and Russian localization tables.
- Verified and ensured default DSP manager settings have echo mix set to 0.0% (neutral and inactive by default across all profiles, resets, and audio converter services).
- Optimized window resize responsiveness across standard and compact player skins:
  - Eliminated continuous delegate churn in `PlaylistTable` and `CompactSkin` during window resizing by binding `effectiveColumns` to discrete breakpoint buckets (`responsiveWidthBucket`), allowing native Qt Quick `RowLayout` to handle continuous width stretching smoothly without re-evaluating or rebuilding delegate models.
  - Fixed automatic column selection in `PlaylistColumnLayoutManager::effectiveVisibleColumns` for narrow viewports and bucket 0, preventing automatic columns from collapsing to minimum widths when available width does not satisfy minimum thresholds.
  - Optimized `WaveformItem` resize performance by deferring multi-megabyte ARGB32 image layer reallocations to a 100ms post-resize timer, seamlessly using fast direct vector drawing during active drag resizing.
- Fixed spurious desktop notifications triggering for the current track when changing playback speed (e.g. holding Space bar for 2x playback or moving the speed slider) while a track is playing.
- Implemented true Varispeed playback across player speed controls and DSP: playback speed changes now proportionally scale pitch (Speed with proportional pitch change), while DSP tempo modifications adjust speed with constant pitch.
- Fixed non-functional and defective parameters across the DSP Manager:
  - **General Tab**: Fixed Pause/Resume fade-in and fade-out (`fadePauseResume`) and Track Navigation fade (`fadeTrackNavigation`). Relocated the DSP audio processing probe to downstream of SoundTouch and peak limiters directly ahead of the audio sink, eliminating 100-300ms buffering delays so fades and volume changes are instantaneous and click-free. Resolved a critical bug in `AudioEngine::fadeOut` where a delayed gain reset timer cancelled active and subsequent `fadeIn` transitions.
  - **Volume Tab**:
    - Functionalized `smoothChanges` with a 48ms smooth volume ramp eliminating slider zipper noise and abrupt audio level clicks.
    - Added `logarithmicControl` applying a perceptual quadratic audio taper for natural volume attenuation.
    - Functionalized `loudnessCompensation` dynamically adjusting bass boost below 0.95 volume according to Fletcher-Munson equal-loudness contours.
    - Verified and connected stereo `balance` panning across all playback streams.
    - Implemented `amplitudeNormalization` (target peak dBFS, preamp, and tag/measured peak) and full `replayGain` processing (auto/track/album modes, preamp, fallback gain, tag extraction from TagLib/GStreamer, and on-the-fly peak analysis) with live diagnostic status strings.
  - **Mixing Tab**:
    - Fixed manual track transitions honoring `mixManualCrossfade`, `mixManualFadeOut`, and `mixManualFadeIn` with configurable durations.
    - Functionalized automatic mixing advance modes, including configurable pause intervals between tracks (`mixAutomaticMode == "pause"`) and smooth crossfade transitions (`mixAutomaticMode == "crossfade"` with auto fade-out before EOS and auto fade-in on the subsequent track).
    - Fixed gapless transition preparation to avoid preempting active DSP mixing modes.
  - **Silence Removal Tab**:
    - Connected real-time frame chunk silence detection in `processDspBuffer` with configurable threshold dBFS and minimum duration.
    - Implemented `silenceRemovalTrimEdges` allowing automatic intro silence skipping, trailing silence elimination to trigger immediate track progression at EOS, and seamless mid-track silence skipping with seek debouncing.
- Fixed desynchronization of playback speed and pitch between keyboard shortcuts and the DSP Manager:
  - Changing playback speed (`[`, `]`, `Backspace`) or tonality/pitch (`-`, `=`, `0`) via shortcut keys now immediately synchronizes with `DspSettingsManager::speed` and `DspSettingsManager::tonalitySemitones`.
  - Unified varispeed playback rate and pitch shifting bidirectionally between `AudioEngine`, `DspSettingsManager`, bottom control bar, standalone player controls, and DSP Manager sliders so values reflect changes in real time across the UI without desynchronization or double-compounding multipliers.

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
