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

log() {
    echo "==> $*"
}

warn() {
    echo "Warning: $*" >&2
}

die() {
    echo "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "$1 is required but not found in PATH."
}

copy_tree() {
    local src="$1"
    local dst="$2"

    [[ -d "${src}" ]] || return 0
    mkdir -p "${dst}"
    cp -a "${src}/." "${dst}/"
}

copy_qt_plugin_subdir() {
    local root="$1"
    local subdir="$2"

    [[ -d "${root}/${subdir}" ]] || return 0
    mkdir -p "${APPDIR}/usr/plugins/${subdir}"
    cp -a "${root}/${subdir}/." "${APPDIR}/usr/plugins/${subdir}/"
}

copy_qml_module() {
    local root="$1"
    local module="$2"

    [[ -d "${root}/${module}" ]] || return 0
    rm -rf "${APPDIR}/usr/qml/${module}"
    mkdir -p "${APPDIR}/usr/qml/${module}"
    cp -a "${root}/${module}/." "${APPDIR}/usr/qml/${module}/"
}

qmake_query() {
    local key="$1"
    if command -v qmake6 >/dev/null 2>&1; then
        qmake6 -query "${key}" | cut -d: -f2-
        return 0
    fi
    return 1
}

pkg_config_query() {
    local package="$1"
    local variable="$2"
    if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists "${package}"; then
        pkg-config --variable="${variable}" "${package}"
        return 0
    fi
    return 1
}

is_elf() {
    readelf -h "$1" >/dev/null 2>&1
}

should_skip_dependency() {
    local dep
    dep="$(readlink -f "$1" 2>/dev/null || printf '%s' "$1")"
    local name
    name="$(basename "${dep}")"

    case "${name}" in
        linux-vdso.so.*|ld-linux*.so*|libc.so.6|libm.so.6|libpthread.so.0|librt.so.1|libdl.so.2|libutil.so.1|libanl.so.1|libresolv.so.2)
            return 0
            ;;
    esac

    return 1
}

copy_host_path() {
    local src="$1"
    local real

    [[ -e "${src}" ]] || return 1
    [[ "${src}" == "${APPDIR}"/* ]] && return 0
    real="$(readlink -f "${src}")"

    local candidate
    for candidate in "${src}" "${real}"; do
        [[ -e "${candidate}" ]] || continue
        [[ "${candidate}" == "${APPDIR}"/* ]] && continue
        if [[ ! -e "${APPDIR}${candidate}" ]]; then
            cp -a --parents "${candidate}" "${APPDIR}"
        fi
    done
}

ldd_dependencies() {
    local target="$1"
    local output
    local status=0
    local timeout_seconds="${APPIMAGE_LDD_TIMEOUT_SECONDS:-10}"

    if command -v timeout >/dev/null 2>&1; then
        output="$(timeout --kill-after=2s "${timeout_seconds}" ldd "${target}" 2>/dev/null)" || status=$?
    else
        output="$(ldd "${target}" 2>/dev/null)" || status=$?
    fi

    if ((status != 0)); then
        if [[ "${status}" -eq 124 || "${status}" -eq 137 ]]; then
            warn "Timed out while scanning dependencies for ${target}"
        fi
        return 0
    fi

    while IFS= read -r line; do
        case "${line}" in
            *"=> not found"*)
                warn "Unresolved dependency for ${target}: ${line}"
                ;;
            *"=>"*"("*)
                local dep="${line#*=> }"
                dep="${dep%% (*}"
                [[ "${dep}" == /* ]] && printf '%s\n' "${dep}"
                ;;
            /*"("*)
                printf '%s\n' "${line%% (*}"
                ;;
        esac
    done <<< "${output}"
}

enqueue_bundle_candidate() {
    local candidate="$1"

    [[ -n "${candidate}" && -e "${candidate}" ]] || return 0

    local key
    key="$(readlink -f "${candidate}" 2>/dev/null || printf '%s' "${candidate}")"
    [[ -n "${seen["${key}"]:-}" || -n "${queued["${key}"]:-}" ]] && return 0

    queue+=("${candidate}")
    queued["${key}"]=1
}

bundle_elf_closure() {
    declare -A seen=()
    declare -A queued=()
    local queue=()
    local processed=0
    local progress_interval="${APPIMAGE_BUNDLE_PROGRESS_INTERVAL:-50}"
    local root
    for root in "$@"; do
        enqueue_bundle_candidate "${root}"
    done

    local queue_index=0
    while ((queue_index < ${#queue[@]})); do
        local file="${queue[queue_index]}"
        ((queue_index += 1))

        [[ -e "${file}" ]] || continue
        is_elf "${file}" || continue

        local file_key
        file_key="$(readlink -f "${file}" 2>/dev/null || printf '%s' "${file}")"
        [[ -n "${seen["${file_key}"]:-}" ]] && continue
        seen["${file_key}"]=1
        ((processed += 1))

        if ((progress_interval > 0 && processed % progress_interval == 0)); then
            log "Bundled dependency scan: ${processed} ELF files processed, ${#queue[@]} queued"
        fi

        while IFS= read -r dep; do
            [[ -n "${dep}" ]] || continue
            should_skip_dependency "${dep}" && continue

            if [[ "${dep}" == "${APPDIR}"/* ]]; then
                enqueue_bundle_candidate "${dep}"
                local dep_resolved
                dep_resolved="$(readlink -f "${dep}" 2>/dev/null || true)"
                if [[ -n "${dep_resolved}" && "${dep_resolved}" == "${APPDIR}"/* ]]; then
                    enqueue_bundle_candidate "${dep_resolved}"
                fi
                continue
            fi

            if ! copy_host_path "${dep}"; then
                warn "Failed to bundle dependency ${dep} required by ${file}"
                continue
            fi

            enqueue_bundle_candidate "${APPDIR}${dep}"

            local resolved
            resolved="$(readlink -f "${dep}" 2>/dev/null || true)"
            if [[ -n "${resolved}" ]]; then
                enqueue_bundle_candidate "${APPDIR}${resolved}"
            fi
        done < <(ldd_dependencies "${file}")
    done

    log "Bundled dependency scan complete: ${processed} ELF files processed"
}

collect_elf_files() {
    local dir="$1"
    [[ -d "${dir}" ]] || return 0
    find "${dir}" -type f \( -perm -u+x -o -name '*.so' -o -name '*.so.*' \) -print
}

bundle_qml_imports() {
    local qml_source_dir="$1"
    local qt_qml_dir="$2"

    if command -v qmlimportscanner >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1; then
        log "Bundling QML imports discovered by qmlimportscanner"

        local qml_paths=()
        mapfile -t qml_paths < <(
            qmlimportscanner -rootPath "${qml_source_dir}" -importPath "${qt_qml_dir}" 2>/dev/null \
            | python3 -c 'import json,sys
items=json.load(sys.stdin)
paths=sorted({item["path"] for item in items
              if item.get("type")=="module"
              and isinstance(item.get("path"), str)})
for path in paths:
    print(path)'
        )

        local path
        for path in "${qml_paths[@]}"; do
            [[ "${path}" == "${qt_qml_dir}"* ]] || continue
            local rel="${path#${qt_qml_dir}/}"
            copy_tree "${path}" "${APPDIR}/usr/qml/${rel}"
        done

        return 0
    fi

    warn "qmlimportscanner/python3 unavailable, bundling entire Qt QML tree."
    copy_tree "${qt_qml_dir}" "${APPDIR}/usr/qml"
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

require_command cmake
require_command appimagetool
require_command ldd
require_command readelf

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

mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd)"
OUTPUT_DIR="$(cd "${OUTPUT_DIR}" && pwd)"

CMAKE_BUILD_DIR="${BUILD_DIR}/cmake-build"
APPDIR="${BUILD_DIR}/AppDir"
APPIMAGE_NAME="${APP_NAME}-${APP_VERSION}-${APPIMAGE_ARCH}.AppImage"
APPIMAGE_PATH="${OUTPUT_DIR}/${APPIMAGE_NAME}"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    rm -rf "${APPDIR}"
    mkdir -p "${APPDIR}"
elif [[ ! -d "${APPDIR}" ]]; then
    die "AppDir not found: ${APPDIR}"$'\n'"Run once without --skip-build to prepare it."
fi

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    log "Configuring CMake (Release)"
    cmake -S "${ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr

    log "Building"
    cmake --build "${CMAKE_BUILD_DIR}" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

    log "Installing into AppDir"
    DESTDIR="${APPDIR}" cmake --install "${CMAKE_BUILD_DIR}"
else
    log "Skipping build/install step (--skip-build)"
fi

APP_BIN="${APPDIR}/usr/bin/${APP_EXEC}"
if [[ ! -x "${APP_BIN}" ]]; then
    die "Expected executable not found: ${APP_BIN}"$'\n'"Run without --skip-build or ensure AppDir is prepared."
fi

QT_PLUGIN_DIR="$(qmake_query QT_INSTALL_PLUGINS || true)"
QT_QML_DIR="$(qmake_query QT_INSTALL_QML || true)"
QT_TRANSLATIONS_DIR="$(qmake_query QT_INSTALL_TRANSLATIONS || true)"
GST_PLUGIN_DIR="$(pkg_config_query gstreamer-1.0 pluginsdir || true)"

[[ -n "${QT_PLUGIN_DIR}" && -d "${QT_PLUGIN_DIR}" ]] || die "Could not determine Qt plugins directory."
[[ -n "${QT_QML_DIR}" && -d "${QT_QML_DIR}" ]] || die "Could not determine Qt QML imports directory."

log "Bundling Qt runtime plugins from ${QT_PLUGIN_DIR}"
QT_PLUGIN_SUBDIRS=(
    platforms
    platformthemes
    platforminputcontexts
    xcbglintegrations
    egldeviceintegrations
    wayland-decoration-client
    wayland-graphics-integration-client
    wayland-shell-integration
    generic
    imageformats
    iconengines
    styles
    multimedia
    tls
    networkinformation
    sqldrivers
    kiconthemes6
)
for qt_plugin_subdir in "${QT_PLUGIN_SUBDIRS[@]}"; do
    copy_qt_plugin_subdir "${QT_PLUGIN_DIR}" "${qt_plugin_subdir}"
done
rm -f \
    "${APPDIR}/usr/plugins/sqldrivers/libqsqlibase.so" \
    "${APPDIR}/usr/plugins/sqldrivers/libqsqlodbc.so" \
    "${APPDIR}/usr/plugins/imageformats/kimg_jxr.so"

bundle_qml_imports "${ROOT_DIR}/qml" "${QT_QML_DIR}"
copy_qml_module "${QT_QML_DIR}" "org/kde/desktop"
copy_qml_module "${QT_QML_DIR}" "org/kde/qqc2desktopstyle"
copy_qml_module "${QT_QML_DIR}" "org/kde/sonnet"

if [[ -n "${QT_TRANSLATIONS_DIR}" && -d "${QT_TRANSLATIONS_DIR}" ]]; then
    log "Bundling Qt translations"
    copy_tree "${QT_TRANSLATIONS_DIR}" "${APPDIR}/usr/translations"
fi

if [[ -n "${GST_PLUGIN_DIR}" && -d "${GST_PLUGIN_DIR}" ]]; then
    log "Bundling GStreamer plugins from ${GST_PLUGIN_DIR}"
    copy_tree "${GST_PLUGIN_DIR}" "${APPDIR}/usr/lib/gstreamer-1.0"
fi

mkdir -p "${APPDIR}/usr/bin"
cat > "${APPDIR}/usr/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Plugins=plugins
QmlImports=qml
Translations=translations
EOF

declare -a bundle_roots=("${APP_BIN}")
while IFS= read -r file; do
    bundle_roots+=("${file}")
done < <(collect_elf_files "${APPDIR}/usr/plugins")
while IFS= read -r file; do
    bundle_roots+=("${file}")
done < <(collect_elf_files "${APPDIR}/usr/qml")
while IFS= read -r file; do
    bundle_roots+=("${file}")
done < <(collect_elf_files "${APPDIR}/usr/lib/gstreamer-1.0")

log "Bundling shared-library closure (${#bundle_roots[@]} roots)"
bundle_elf_closure "${bundle_roots[@]}"

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
    warn "icon not found at ${ICON_SRC}, AppImage will have no bundled icon."
fi

cat > "${APPDIR}/AppRun" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

APPDIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export APPDIR
export PATH="${APPDIR}/usr/bin:${APPDIR}/usr/lib/gstreamer-1.0:${PATH}"

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
append_ld_path "${APPDIR}/usr/lib/libproxy"
append_ld_path "${APPDIR}/usr/lib/pulseaudio"
append_ld_path "${APPDIR}/usr/lib/x86_64-linux-gnu"
append_ld_path "${APPDIR}/usr/lib/x86_64-linux-gnu/libproxy"
append_ld_path "${APPDIR}/usr/lib/x86_64-linux-gnu/pulseaudio"
append_ld_path "${APPDIR}/usr/lib/aarch64-linux-gnu"
append_ld_path "${APPDIR}/usr/lib/aarch64-linux-gnu/libproxy"
append_ld_path "${APPDIR}/usr/lib/aarch64-linux-gnu/pulseaudio"
append_ld_path "${APPDIR}/lib"
append_ld_path "${APPDIR}/lib64"
append_ld_path "${APPDIR}/lib/x86_64-linux-gnu"
append_ld_path "${APPDIR}/lib/x86_64-linux-gnu/libproxy"
append_ld_path "${APPDIR}/lib/x86_64-linux-gnu/pulseaudio"
append_ld_path "${APPDIR}/lib/aarch64-linux-gnu"
append_ld_path "${APPDIR}/lib/aarch64-linux-gnu/libproxy"
append_ld_path "${APPDIR}/lib/aarch64-linux-gnu/pulseaudio"

export QT_PLUGIN_PATH="${APPDIR}/usr/plugins${QT_PLUGIN_PATH:+:${QT_PLUGIN_PATH}}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${APPDIR}/usr/plugins/platforms"
export QML2_IMPORT_PATH="${APPDIR}/usr/qml${QML2_IMPORT_PATH:+:${QML2_IMPORT_PATH}}"
export XDG_DATA_DIRS="${APPDIR}/usr/share${XDG_DATA_DIRS:+:${XDG_DATA_DIRS}}"

if [[ -d "${APPDIR}/usr/lib/gstreamer-1.0" ]]; then
    export GST_PLUGIN_PATH_1_0="${APPDIR}/usr/lib/gstreamer-1.0"
    export GST_PLUGIN_SYSTEM_PATH_1_0="${APPDIR}/usr/lib/gstreamer-1.0"
    if [[ -x "${APPDIR}/usr/lib/gstreamer-1.0/gst-plugin-scanner" ]]; then
        export GST_PLUGIN_SCANNER_1_0="${APPDIR}/usr/lib/gstreamer-1.0/gst-plugin-scanner"
        export GST_PLUGIN_SCANNER="${APPDIR}/usr/lib/gstreamer-1.0/gst-plugin-scanner"
    fi
fi

exec "${APPDIR}/usr/bin/waveflux" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

log "Building AppImage: ${APPIMAGE_PATH}"
APPIMAGETOOL_ARGS=(-n)
if [[ -n "${RUNTIME_FILE}" ]]; then
    [[ -f "${RUNTIME_FILE}" ]] || die "Runtime file does not exist: ${RUNTIME_FILE}"
    APPIMAGETOOL_ARGS+=(--runtime-file "${RUNTIME_FILE}")
fi

if ! ARCH="${APPIMAGE_ARCH}" appimagetool "${APPIMAGETOOL_ARGS[@]}" "${APPDIR}" "${APPIMAGE_PATH}"; then
    if [[ -z "${RUNTIME_FILE}" ]]; then
        warn "appimagetool failed. If network is restricted, pass --runtime-file <path>."
    fi
    exit 1
fi

echo "Done: ${APPIMAGE_PATH}"
