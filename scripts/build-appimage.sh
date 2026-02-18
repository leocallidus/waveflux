#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-appimage.sh [options]

Options:
  --build-dir <dir>    Build workspace (default: ./build-appimage)
  --output-dir <dir>   Output directory for .AppImage (default: ./dist)
  --version <value>    Override app version in file name
  --runtime-file <f>   Local AppImage runtime file (offline-friendly)
  --skip-build         Skip CMake configure/build/install step
  -h, --help           Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

APP_NAME="WaveFlux"
APP_ID="waveflux"
APP_EXEC="waveflux"
DEFAULT_BUILD_DIR="${ROOT_DIR}/build-appimage"
DEFAULT_OUTPUT_DIR="${ROOT_DIR}/dist"

BUILD_DIR="${DEFAULT_BUILD_DIR}"
OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
SKIP_BUILD=0
VERSION_OVERRIDE=""
RUNTIME_FILE="${APPIMAGE_RUNTIME_FILE:-}"

while (($# > 0)); do
    case "$1" in
        --build-dir)
            BUILD_DIR="${2:?Missing value for --build-dir}"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="${2:?Missing value for --output-dir}"
            shift 2
            ;;
        --version)
            VERSION_OVERRIDE="${2:?Missing value for --version}"
            shift 2
            ;;
        --runtime-file)
            RUNTIME_FILE="${2:?Missing value for --runtime-file}"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but not found in PATH." >&2
    exit 1
fi
if ! command -v appimagetool >/dev/null 2>&1; then
    echo "appimagetool is required but not found in PATH." >&2
    exit 1
fi

PROJECT_VERSION="$(
    sed -nE 's/^[[:space:]]*project\([^)]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${ROOT_DIR}/CMakeLists.txt" | head -n1
)"
if [[ -z "${PROJECT_VERSION}" ]]; then
    PROJECT_VERSION="0.0.0"
fi
APP_VERSION="${VERSION_OVERRIDE:-${PROJECT_VERSION}}"

HOST_ARCH="$(uname -m)"
case "${HOST_ARCH}" in
    x86_64|amd64) APPIMAGE_ARCH="x86_64" ;;
    aarch64|arm64) APPIMAGE_ARCH="aarch64" ;;
    armv7l|armhf) APPIMAGE_ARCH="armhf" ;;
    i686|i386) APPIMAGE_ARCH="i686" ;;
    *) APPIMAGE_ARCH="${HOST_ARCH}" ;;
esac

CMAKE_BUILD_DIR="${BUILD_DIR}/cmake-build"
APPDIR="${BUILD_DIR}/AppDir"
APPIMAGE_NAME="${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"
APPIMAGE_PATH="${OUTPUT_DIR}/${APPIMAGE_NAME}"

mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    rm -rf "${APPDIR}"
    mkdir -p "${APPDIR}"
elif [[ ! -d "${APPDIR}" ]]; then
    echo "AppDir not found: ${APPDIR}" >&2
    echo "Run once without --skip-build to prepare it." >&2
    exit 1
fi

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    echo "==> Configuring CMake (Release)"
    cmake -S "${ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr

    echo "==> Building"
    cmake --build "${CMAKE_BUILD_DIR}" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

    echo "==> Installing into AppDir"
    DESTDIR="${APPDIR}" cmake --install "${CMAKE_BUILD_DIR}"
else
    echo "==> Skipping build/install step (--skip-build)"
fi

APP_BIN="${APPDIR}/usr/bin/${APP_EXEC}"
if [[ ! -x "${APP_BIN}" ]]; then
    echo "Expected executable not found: ${APP_BIN}" >&2
    echo "Run without --skip-build or ensure AppDir is prepared." >&2
    exit 1
fi

mkdir -p "${APPDIR}/usr/share/applications"
cat > "${APPDIR}/${APP_ID}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=${APP_NAME}
Comment=WaveFlux audio player
Exec=${APP_EXEC}
Icon=${APP_ID}
Terminal=false
Categories=AudioVideo;Audio;Player;
StartupNotify=true
EOF
cp "${APPDIR}/${APP_ID}.desktop" "${APPDIR}/usr/share/applications/${APP_ID}.desktop"

ICON_SRC="${ROOT_DIR}/resources/icons/waveflux.svg"
if [[ -f "${ICON_SRC}" ]]; then
    cp "${ICON_SRC}" "${APPDIR}/${APP_ID}.svg"
    ln -sfn "${APP_ID}.svg" "${APPDIR}/.DirIcon"
    mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
    cp "${ICON_SRC}" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/${APP_ID}.svg"
else
    echo "Warning: icon not found at ${ICON_SRC}, AppImage will have no bundled icon." >&2
fi

cat > "${APPDIR}/AppRun" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

APPDIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export APPDIR
export PATH="${APPDIR}/usr/bin:${PATH}"

append_ld_path() {
    local dir="$1"
    if [[ -d "${dir}" ]]; then
        if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
            export LD_LIBRARY_PATH="${dir}:${LD_LIBRARY_PATH}"
        else
            export LD_LIBRARY_PATH="${dir}"
        fi
    fi
}

append_ld_path "${APPDIR}/usr/lib"
append_ld_path "${APPDIR}/usr/lib64"
append_ld_path "${APPDIR}/usr/lib/x86_64-linux-gnu"

if [[ -d "${APPDIR}/usr/plugins" ]]; then
    export QT_PLUGIN_PATH="${APPDIR}/usr/plugins"
fi
if [[ -d "${APPDIR}/usr/qml" ]]; then
    export QML2_IMPORT_PATH="${APPDIR}/usr/qml"
fi

exec "${APPDIR}/usr/bin/waveflux" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

echo "==> Building AppImage: ${APPIMAGE_PATH}"
APPIMAGETOOL_ARGS=(-n)
if [[ -n "${RUNTIME_FILE}" ]]; then
    if [[ ! -f "${RUNTIME_FILE}" ]]; then
        echo "Runtime file does not exist: ${RUNTIME_FILE}" >&2
        exit 1
    fi
    APPIMAGETOOL_ARGS+=(--runtime-file "${RUNTIME_FILE}")
fi

if ! ARCH="${APPIMAGE_ARCH}" appimagetool "${APPIMAGETOOL_ARGS[@]}" "${APPDIR}" "${APPIMAGE_PATH}"; then
    if [[ -z "${RUNTIME_FILE}" ]]; then
        echo "appimagetool failed. If network is restricted, pass --runtime-file <path>." >&2
    fi
    exit 1
fi

echo "Done: ${APPIMAGE_PATH}"
