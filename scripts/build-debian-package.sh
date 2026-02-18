#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-debian-package.sh [options]

Builds a local Debian package (*.deb) using CMake install + dpkg-deb.

Options:
  --work-dir <dir>      Working directory (default: ./build-debian)
  --output-dir <dir>    Output directory for package artifact (default: ./dist/debian)
  --package-name <name> Debian package name (default: waveflux)
  --version <value>     Override upstream version from CMakeLists.txt (e.g. 1.2.3)
  --release <n>         Debian revision suffix (default: 1)
  --arch <arch>         Override Debian architecture (default: dpkg --print-architecture)
  --maintainer <value>  Maintainer field value (default: WaveFlux Maintainers <noreply@example.com>)
  --depends <list>      Space- or comma-separated dependencies for control file
  --skip-build          Skip CMake configure/build/install and package existing staging tree
  --no-clean            Keep previous working directory contents
  -h, --help            Show this help
EOF
}

print_debian_build_requirements_hint() {
    cat >&2 <<'EOF'
Missing Debian build dependencies for full build mode.
Install them with:
  sudo apt update
  sudo apt install -y \
    build-essential cmake ninja-build pkgconf \
    qt6-base-dev qt6-declarative-dev \
    extra-cmake-modules libkirigami-dev libkf6coreaddons-dev libkf6i18n-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libtag-dev

Or run with --skip-build after preparing build-debian/pkgroot.
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

DEFAULT_WORK_DIR="${ROOT_DIR}/build-debian"
DEFAULT_OUTPUT_DIR="${ROOT_DIR}/dist/debian"

WORK_DIR="${DEFAULT_WORK_DIR}"
OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
PACKAGE_NAME="waveflux"
VERSION_OVERRIDE=""
DEB_RELEASE="1"
ARCH_OVERRIDE=""
MAINTAINER="WaveFlux Maintainers <noreply@example.com>"
DEFAULT_DEPENDS="libqt6core6 | libqt6core6t64, libqt6gui6, libqt6qml6, libqt6quick6, libqt6sql6, libqt6widgets6, libqt6dbus6, libqt6network6, libqt6opengl6, libqt6concurrent6 | libqt6core6 | libqt6core6t64, libqt6quickcontrols2-6, libkirigami6, libkirigamiplatform6, libkf6coreaddons6, libkf6i18n6, libgstreamer1.0-0, libgstreamer-plugins-base1.0-0, libtag2 | libtag1v5 | libtag1-dev, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-dialogs, qml6-module-org-kde-kirigami"
DEPENDS_OVERRIDE=""
SKIP_BUILD=0
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
        --package-name)
            PACKAGE_NAME="${2:?Missing value for --package-name}"
            shift 2
            ;;
        --version)
            VERSION_OVERRIDE="${2:?Missing value for --version}"
            shift 2
            ;;
        --release)
            DEB_RELEASE="${2:?Missing value for --release}"
            shift 2
            ;;
        --arch)
            ARCH_OVERRIDE="${2:?Missing value for --arch}"
            shift 2
            ;;
        --maintainer)
            MAINTAINER="${2:?Missing value for --maintainer}"
            shift 2
            ;;
        --depends)
            DEPENDS_OVERRIDE="${2:?Missing value for --depends}"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
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

if [[ "${SKIP_BUILD}" -eq 1 && "${NO_CLEAN}" -eq 0 ]]; then
    echo "==> --skip-build implies preserving existing staging files; enabling --no-clean"
    NO_CLEAN=1
fi

DEPENDS_FIELD="${DEPENDS_OVERRIDE:-${DEFAULT_DEPENDS}}"
if [[ "${DEPENDS_FIELD}" != *","* ]]; then
    DEPENDS_FIELD="$(echo "${DEPENDS_FIELD}" | xargs)"
    DEPENDS_FIELD="${DEPENDS_FIELD// /, }"
fi

for cmd in cmake dpkg-deb sed awk install xargs; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "Required command not found: ${cmd}" >&2
        exit 1
    fi
done

if [[ "${SKIP_BUILD}" -eq 0 && -x /usr/bin/dpkg-query ]]; then
    REQUIRED_BUILD_PACKAGES=(
        build-essential
        cmake
        ninja-build
        pkgconf
        qt6-base-dev
        qt6-declarative-dev
        extra-cmake-modules
        libkirigami-dev
        libkf6coreaddons-dev
        libkf6i18n-dev
        libgstreamer1.0-dev
        libgstreamer-plugins-base1.0-dev
        libtag-dev
    )

    MISSING_BUILD_PACKAGES=()
    for pkg in "${REQUIRED_BUILD_PACKAGES[@]}"; do
        if ! dpkg-query -W -f='${Status}' "${pkg}" 2>/dev/null | grep -q 'install ok installed'; then
            MISSING_BUILD_PACKAGES+=("${pkg}")
        fi
    done

    if ((${#MISSING_BUILD_PACKAGES[@]} > 0)); then
        echo "Missing packages: ${MISSING_BUILD_PACKAGES[*]}" >&2
        print_debian_build_requirements_hint
        exit 1
    fi
fi

PROJECT_VERSION="$(
    sed -nE 's/^[[:space:]]*project\([^)]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${ROOT_DIR}/CMakeLists.txt" | head -n1
)"
if [[ -z "${PROJECT_VERSION}" ]]; then
    echo "Could not detect project version from CMakeLists.txt" >&2
    exit 1
fi

UPSTREAM_VERSION="${VERSION_OVERRIDE:-${PROJECT_VERSION}}"
DEB_VERSION="${UPSTREAM_VERSION}-${DEB_RELEASE}"

if [[ -n "${ARCH_OVERRIDE}" ]]; then
    DEB_ARCH="${ARCH_OVERRIDE}"
elif command -v dpkg >/dev/null 2>&1; then
    DEB_ARCH="$(dpkg --print-architecture)"
else
    case "$(uname -m)" in
        x86_64|amd64) DEB_ARCH="amd64" ;;
        aarch64|arm64) DEB_ARCH="arm64" ;;
        armv7l|armhf) DEB_ARCH="armhf" ;;
        i686|i386) DEB_ARCH="i386" ;;
        *) DEB_ARCH="$(uname -m)" ;;
    esac
fi

DESKTOP_FILE="${ROOT_DIR}/packaging/debian/waveflux.desktop"
if [[ ! -f "${DESKTOP_FILE}" ]]; then
    DESKTOP_FILE="${ROOT_DIR}/packaging/aur/waveflux.desktop"
fi
if [[ ! -f "${DESKTOP_FILE}" ]]; then
    echo "Desktop file not found in packaging/debian or packaging/aur." >&2
    exit 1
fi

ICON_FILE="${ROOT_DIR}/resources/icons/waveflux.svg"
if [[ ! -f "${ICON_FILE}" ]]; then
    echo "Icon file missing: ${ICON_FILE}" >&2
    exit 1
fi

CMAKE_BUILD_DIR="${WORK_DIR}/cmake-build"
PKG_ROOT="${WORK_DIR}/pkgroot"
DEBIAN_DIR="${PKG_ROOT}/DEBIAN"
DEB_FILE="${PACKAGE_NAME}_${DEB_VERSION}_${DEB_ARCH}.deb"
DEB_PATH="${OUTPUT_DIR}/${DEB_FILE}"

mkdir -p "${OUTPUT_DIR}"
if [[ "${NO_CLEAN}" -eq 0 ]]; then
    rm -rf "${WORK_DIR}"
fi
mkdir -p "${WORK_DIR}"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
    rm -rf "${PKG_ROOT}"
    mkdir -p "${PKG_ROOT}"

    echo "==> Configuring CMake (Release)"
    cmake -S "${ROOT_DIR}" -B "${CMAKE_BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr

    echo "==> Building"
    cmake --build "${CMAKE_BUILD_DIR}" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

    echo "==> Installing into staging root"
    DESTDIR="${PKG_ROOT}" cmake --install "${CMAKE_BUILD_DIR}"
else
    echo "==> Skipping build/install step (--skip-build)"
    if [[ ! -d "${PKG_ROOT}" ]]; then
        echo "Staging root not found: ${PKG_ROOT}" >&2
        echo "Run once without --skip-build to prepare it." >&2
        exit 1
    fi
fi

APP_BIN="${PKG_ROOT}/usr/bin/waveflux"
if [[ ! -x "${APP_BIN}" ]]; then
    echo "Expected executable not found: ${APP_BIN}" >&2
    exit 1
fi

install -Dm644 "${DESKTOP_FILE}" \
    "${PKG_ROOT}/usr/share/applications/waveflux.desktop"
install -Dm644 "${ICON_FILE}" \
    "${PKG_ROOT}/usr/share/icons/hicolor/scalable/apps/waveflux.svg"
install -Dm644 "${ROOT_DIR}/README.md" \
    "${PKG_ROOT}/usr/share/doc/${PACKAGE_NAME}/README.md"

mkdir -p "${DEBIAN_DIR}"

INSTALLED_SIZE="$(du -sk "${PKG_ROOT}" | awk '{print $1}')"

{
    echo "Package: ${PACKAGE_NAME}"
    echo "Version: ${DEB_VERSION}"
    echo "Section: sound"
    echo "Priority: optional"
    echo "Architecture: ${DEB_ARCH}"
    echo "Depends: ${DEPENDS_FIELD}"
    echo "Maintainer: ${MAINTAINER}"
    echo "Homepage: https://github.com/leocallidus/waveflux"
    echo "Installed-Size: ${INSTALLED_SIZE}"
    echo "Description: Minimalist desktop audio player with waveform navigation"
    echo " WaveFlux is a Linux desktop audio player focused on fast"
    echo " local-library workflows and waveform-based navigation."
} > "${DEBIAN_DIR}/control"

chmod 0755 "${DEBIAN_DIR}"
chmod 0644 "${DEBIAN_DIR}/control"

echo "==> Building Debian package: ${DEB_PATH}"
dpkg-deb --build --root-owner-group "${PKG_ROOT}" "${DEB_PATH}"

echo "Done: ${DEB_PATH}"
