# WaveFlux

WaveFlux is a Linux desktop audio player built with Qt 6, Kirigami, and GStreamer.  
It focuses on fast local-library workflows, waveform-based navigation, and precise playback controls.

## Features

- Waveform-first playback UI with zooming, scrubbing, and CUE segment overlays
- Gapless playback flow with queue ("Up Next"), repeat modes, and shuffle
- Deterministic shuffle mode with configurable seed and repeatability behavior
- Speed, pitch, reverse playback, and selectable audio quality profiles
- 10-band equalizer with built-in presets and user preset import/export (JSON)
- Playlist ingest from files, folders, drag-and-drop, and URLs
- XSPF playlist import and CUE sheet parsing
- Playlist export to M3U/M3U8 and JSON
- Rich metadata support (title, artist, album, bitrate, sample rate, bit depth, BPM, album art)
- Single-track and bulk tag editing via TagLib, including cover art updates
- Smart collections and persisted library/search backend using SQLite
- Saved playlist profiles with restore/update/duplicate/delete workflow
- Session persistence (playlist, playback context, position)
- Linux desktop integration: MPRIS (D-Bus), tray controls, XDG portal file dialogs, trash/file-manager actions
- Built-in performance profiler overlay with JSON/CSV/bundle export
- UI languages: English and Russian
- Two interface modes: normal and compact

## Technology Stack

- C++20
- Qt 6.5 (Core, Quick, QML, GUI, SQL, D-Bus, Concurrent, Widgets, Test, QuickControls2)
- KDE Frameworks 6 (Kirigami, CoreAddons, I18n)
- GStreamer 1.0 (`gstreamer-1.0`, `gstreamer-app-1.0`, `gstreamer-audio-1.0`)
- TagLib
- CMake (3.21+)

## Build Requirements

At minimum, install:

- A C++20 compiler (`gcc`/`clang`)
- `cmake` (3.21 or newer)
- `pkgconf`/`pkg-config`
- Qt 6 development packages listed above
- KF6 development packages listed above
- GStreamer development packages listed above
- TagLib development package

Arch Linux reference install:

```bash
sudo pacman -S --needed \
  cmake ninja gcc pkgconf \
  qt6-base qt6-declarative qt6-quickcontrols2 \
  kirigami kcoreaddons ki18n \
  gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad \
  taglib
```

Optional codec/plugin pack:

```bash
sudo pacman -S --needed gst-plugins-ugly
```

Fedora/RHEL reference install:

```bash
sudo dnf install -y \
  cmake gcc-c++ ninja-build pkgconf-pkg-config rpm-build \
  qt6-qtbase-devel qt6-qtdeclarative-devel \
  kf6-kirigami-devel kf6-kcoreaddons-devel kf6-ki18n-devel \
  gstreamer1-devel gstreamer1-plugins-base-devel \
  taglib-devel
```

Recommended runtime codec/plugin packs:

```bash
sudo dnf install -y \
  gstreamer1-plugins-good gstreamer1-plugins-bad-free \
  gstreamer1-plugins-ugly
```

## Build and Run

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j"$(nproc)"
./build/waveflux
```

Optional local install:

```bash
cmake --install build --prefix ~/.local
```

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

Current test targets include:

- `tst_equalizer_preset_manager`
- `tst_cue_sheet_parser`
- `tst_xspf_playlist_parser`
- `tst_app_settings_manager`
- `tst_playback_controller_transitions`
- `tst_playback_controller_scenarios`

## Packaging

Build AppImage:

```bash
./scripts/build-appimage.sh
```

Output: `dist/WaveFlux-<version>-<arch>.AppImage`

Build local Arch package (`.pkg.tar.zst`):

```bash
./scripts/build-pacman-package.sh --syncdeps
```

Output: `dist/pacman/`

Build local Debian package (`.deb`):

```bash
./scripts/build-debian-package.sh
```

If build dependencies are missing, install them and retry:

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build pkgconf \
  qt6-base-dev qt6-declarative-dev \
  extra-cmake-modules libkirigami-dev libkf6coreaddons-dev libkf6i18n-dev \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  libtag-dev
```

The Debian control file runtime dependency list can be overridden with `--depends`.

Output: `dist/debian/`

Build local RPM package (`.rpm`):

```bash
./scripts/build-rpm-package.sh
```

If build dependencies are missing, install them automatically:

```bash
./scripts/build-rpm-package.sh --install-build-deps
```

Output: `dist/rpm/`

## Usage Notes

- Open files: `Ctrl+O`
- Add folder: `Ctrl+Shift+O`
- Export playlist: `Ctrl+E`
- Play/Pause: `Space`
- Fullscreen: `F11`
- Shortcuts reference dialog: `F1`
- Equalizer dialog: `Ctrl+Shift+G`

Search supports field-aware tokens and quick filters, for example:

- `title:night`
- `artist:massive attack`
- `album:mezzanine`
- `path:/music/live`
- `is:lossless`
- `is:hires`

## Troubleshooting

- Equalizer unavailable: install the GStreamer equalizer plugin (`equalizer-nbands`), usually via `gst-plugins-good`/`gst-plugins-bad` depending on distro packaging.
- AppImage build fails in restricted/offline environments: use `scripts/build-appimage.sh --runtime-file <path-to-runtime>`.
- Tray option is disabled in settings: your desktop session may not provide a tray host.

## Project Structure

- `src/` - C++ backend (audio engine, models, playback, integration services)
- `src/library/` - SQLite library/search/smart-collection backend
- `qml/` - QML/Kirigami UI
- `tests/` - Qt Test targets
- `scripts/` - packaging/build helper scripts
- `packaging/aur/` - Arch Linux package assets
- `packaging/debian/` - Debian package assets
- `packaging/rpm/` - RPM spec template assets
- `resources/icons/` - app icons

## License

MIT (declared in packaging metadata).
