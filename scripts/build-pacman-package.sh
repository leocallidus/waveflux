#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-pacman-package.sh [options]

Builds a local Arch package (*.pkg.tar.zst) using makepkg.

Options:
  --work-dir <dir>     Working directory (default: ./build-pacman)
  --output-dir <dir>   Output directory for package artifact (default: ./dist/pacman)
  --pkgrel <n>         PKGBUILD pkgrel value (default: 1)
  --syncdeps           Run makepkg with --syncdeps
  --include-debug      Also copy *-debug.pkg.tar.zst if produced
  --no-clean           Keep previous working directory contents
  -h, --help           Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
DEFAULT_WORK_DIR="${ROOT_DIR}/build-pacman"
DEFAULT_OUTPUT_DIR="${ROOT_DIR}/dist/pacman"

WORK_DIR="${DEFAULT_WORK_DIR}"
OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
PKGREL="1"
SYNCDEPS=0
INCLUDE_DEBUG=0
NO_CLEAN=0

while (($# > 0)); do
    case "$1" in
        --work-dir)
            WORK_DIR="${2:?Missing value for --work-dir}"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="${2:?Missing value for --output-dir}"
            shift 2
            ;;
        --pkgrel)
            PKGREL="${2:?Missing value for --pkgrel}"
            shift 2
            ;;
        --syncdeps)
            SYNCDEPS=1
            shift
            ;;
        --include-debug)
            INCLUDE_DEBUG=1
            shift
            ;;
        --no-clean)
            NO_CLEAN=1
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

for cmd in makepkg cmake tar sha256sum awk; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "Required command not found: ${cmd}" >&2
        exit 1
    fi
done

PROJECT_VERSION="$(
    sed -nE 's/^[[:space:]]*project\([^)]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${ROOT_DIR}/CMakeLists.txt" | head -n1
)"
if [[ -z "${PROJECT_VERSION}" ]]; then
    echo "Could not detect project version from CMakeLists.txt" >&2
    exit 1
fi

PKGNAME="waveflux"
SRC_BASENAME="${PKGNAME}-${PROJECT_VERSION}"
SRC_ARCHIVE="${SRC_BASENAME}.tar.gz"
DESKTOP_FILE="${ROOT_DIR}/packaging/aur/waveflux.desktop"

if [[ ! -f "${DESKTOP_FILE}" ]]; then
    echo "Desktop file missing: ${DESKTOP_FILE}" >&2
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"
if [[ "${NO_CLEAN}" -eq 0 ]]; then
    rm -rf "${WORK_DIR}"
fi
mkdir -p "${WORK_DIR}"

SRC_ARCHIVE_PATH="${WORK_DIR}/${SRC_ARCHIVE}"
PKGBUILD_PATH="${WORK_DIR}/PKGBUILD"

echo "==> Creating source archive: ${SRC_ARCHIVE_PATH}"
tar -C "${ROOT_DIR}" \
    --exclude-vcs \
    --exclude='.idea' \
    --exclude='build' \
    --exclude='build-*' \
    --exclude='cmake-build-*' \
    --exclude='build-appimage' \
    --exclude='build-pacman' \
    --exclude='dist' \
    --exclude='.build-phase0' \
    --exclude='*.pkg.tar.zst' \
    --exclude='*.AppImage' \
    --transform="s,^\.,${SRC_BASENAME}," \
    -czf "${SRC_ARCHIVE_PATH}" .

cp "${DESKTOP_FILE}" "${WORK_DIR}/waveflux.desktop"

SRC_SHA256="$(sha256sum "${SRC_ARCHIVE_PATH}" | awk '{print $1}')"
DESKTOP_SHA256="$(sha256sum "${WORK_DIR}/waveflux.desktop" | awk '{print $1}')"

cat > "${PKGBUILD_PATH}" <<EOF
pkgname=${PKGNAME}
pkgver=${PROJECT_VERSION}
pkgrel=${PKGREL}
pkgdesc="Minimalist desktop audio player with dynamic waveform visualization"
arch=('x86_64' 'aarch64')
url="https://github.com/leocallidus/waveflux"
license=('MIT')
options=('!debug')
depends=(
  'qt6-base'
  'qt6-declarative'
  'kirigami'
  'kcoreaddons'
  'ki18n'
  'gstreamer'
  'gst-plugins-base'
  'gst-plugins-good'
  'gst-plugins-bad'
  'taglib'
)
makedepends=(
  'cmake'
  'ninja'
  'pkgconf'
  'gcc'
)
optdepends=(
  'gst-plugins-ugly: extra proprietary codec support'
)
source=(
  '${SRC_ARCHIVE}'
  'waveflux.desktop'
)
sha256sums=(
  '${SRC_SHA256}'
  '${DESKTOP_SHA256}'
)

build() {
  cmake -S "\${srcdir}/${SRC_BASENAME}" \
        -B "\${srcdir}/build" \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=OFF
  cmake --build "\${srcdir}/build"
}

package() {
  DESTDIR="\${pkgdir}" cmake --install "\${srcdir}/build"

  install -Dm644 "\${srcdir}/waveflux.desktop" \
    "\${pkgdir}/usr/share/applications/waveflux.desktop"
  install -Dm644 "\${srcdir}/${SRC_BASENAME}/resources/icons/waveflux.svg" \
    "\${pkgdir}/usr/share/icons/hicolor/scalable/apps/waveflux.svg"
  install -Dm644 "\${srcdir}/${SRC_BASENAME}/README.md" \
    "\${pkgdir}/usr/share/doc/waveflux/README.md"
}
EOF

MAKEPKG_ARGS=(--clean --cleanbuild --force --noconfirm)
if [[ "${SYNCDEPS}" -eq 1 ]]; then
    MAKEPKG_ARGS+=(--syncdeps)
fi

echo "==> Running makepkg in ${WORK_DIR}"
(
    cd "${WORK_DIR}"
    makepkg "${MAKEPKG_ARGS[@]}"
)

mapfile -t BUILT_PACKAGES < <(find "${WORK_DIR}" -maxdepth 1 -type f -name '*.pkg.tar.zst' -print | sort)
if [[ "${#BUILT_PACKAGES[@]}" -eq 0 ]]; then
    echo "Package artifact was not produced." >&2
    exit 1
fi

MAIN_PACKAGE=""
for pkg_path in "${BUILT_PACKAGES[@]}"; do
    pkg_file="$(basename -- "${pkg_path}")"
    if [[ "${pkg_file}" == "${PKGNAME}-"* && "${pkg_file}" != "${PKGNAME}-debug-"* ]]; then
        MAIN_PACKAGE="${pkg_path}"
        break
    fi
done
if [[ -z "${MAIN_PACKAGE}" ]]; then
    MAIN_PACKAGE="${BUILT_PACKAGES[0]}"
fi

cp -f "${MAIN_PACKAGE}" "${OUTPUT_DIR}/"
echo "Done: ${OUTPUT_DIR}/$(basename -- "${MAIN_PACKAGE}")"

if [[ "${INCLUDE_DEBUG}" -eq 1 ]]; then
    for pkg_path in "${BUILT_PACKAGES[@]}"; do
        pkg_file="$(basename -- "${pkg_path}")"
        if [[ "${pkg_file}" == "${PKGNAME}-debug-"* ]]; then
            cp -f "${pkg_path}" "${OUTPUT_DIR}/"
            echo "Done (debug): ${OUTPUT_DIR}/${pkg_file}"
        fi
    done
fi
