#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-flatpak.sh [options]

Builds a Flatpak single-file bundle (*.flatpak) using flatpak-builder.

Options:
  --build-dir <dir>    Working build directory (default: ./build-flatpak)
  --output-dir <dir>   Output directory for .flatpak bundle (default: ./dist/flatpak)
  --repo-dir <dir>     OSTree repository directory (default: <build-dir>/repo)
  --arch <arch>        Target architecture (x86_64, aarch64) (default: host arch)
  --install            Install the built Flatpak locally
  --no-clean           Keep existing build directory
  -h, --help           Show this help
EOF
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
DEFAULT_WORK_DIR="${ROOT_DIR}/build-flatpak"
DEFAULT_OUTPUT_DIR="${ROOT_DIR}/dist/flatpak"

WORK_DIR="${DEFAULT_WORK_DIR}"
OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
REPO_DIR=""
TARGET_ARCH=""
INSTALL_LOCAL=0
NO_CLEAN=0

while (($# > 0)); do
    case "$1" in
        --build-dir)
            WORK_DIR="${2:?Missing value for --build-dir}"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="${2:?Missing value for --output-dir}"
            shift 2
            ;;
        --repo-dir)
            REPO_DIR="${2:?Missing value for --repo-dir}"
            shift 2
            ;;
        --arch)
            TARGET_ARCH="${2:?Missing value for --arch}"
            shift 2
            ;;
        --install)
            INSTALL_LOCAL=1
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

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Required tool not found: $1" >&2
        exit 1
    fi
}

require_cmd flatpak
require_cmd flatpak-builder

if [[ -z "${TARGET_ARCH}" ]]; then
    HOST_ARCH="$(uname -m)"
    case "${HOST_ARCH}" in
        x86_64|amd64) TARGET_ARCH="x86_64" ;;
        aarch64|arm64) TARGET_ARCH="aarch64" ;;
        *) TARGET_ARCH="${HOST_ARCH}" ;;
    esac
fi

if [[ -z "${REPO_DIR}" ]]; then
    REPO_DIR="${WORK_DIR}/repo"
fi

MANIFEST_PATH="${ROOT_DIR}/packaging/flatpak/io.github.leocallidus.waveflux.yml"
if [[ ! -f "${MANIFEST_PATH}" ]]; then
    echo "Flatpak manifest missing: ${MANIFEST_PATH}" >&2
    exit 1
fi

PROJECT_VERSION="$(
    sed -nE 's/^[[:space:]]*project\([^)]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
        "${ROOT_DIR}/CMakeLists.txt" | head -n1
)"
PROJECT_VERSION="${PROJECT_VERSION:-1.4.1}"
BUNDLE_NAME="WaveFlux-${PROJECT_VERSION}-${TARGET_ARCH}.flatpak"
BUNDLE_PATH="${OUTPUT_DIR}/${BUNDLE_NAME}"

mkdir -p "${OUTPUT_DIR}"
if [[ "${NO_CLEAN}" -eq 0 ]]; then
    rm -rf "${WORK_DIR}/app"
fi
mkdir -p "${WORK_DIR}" "${REPO_DIR}"

BUILDER_ARGS=(
    --force-clean
    --arch="${TARGET_ARCH}"
    --repo="${REPO_DIR}"
    --state-dir="${WORK_DIR}/.flatpak-builder"
)

if [[ "${INSTALL_LOCAL}" -eq 1 ]]; then
    BUILDER_ARGS+=(--install --user)
fi

echo "==> Building Flatpak in ${WORK_DIR}/app..."
flatpak-builder "${BUILDER_ARGS[@]}" "${WORK_DIR}/app" "${MANIFEST_PATH}"

echo "==> Creating Flatpak single-file bundle: ${BUNDLE_PATH}..."
flatpak build-bundle \
    --arch="${TARGET_ARCH}" \
    "${REPO_DIR}" \
    "${BUNDLE_PATH}" \
    io.github.leocallidus.waveflux

# Provide a normalized filename link as well
cp -f "${BUNDLE_PATH}" "${OUTPUT_DIR}/waveflux-${TARGET_ARCH}.flatpak"

echo "Done: ${BUNDLE_PATH}"
echo "Normalized: ${OUTPUT_DIR}/waveflux-${TARGET_ARCH}.flatpak"
