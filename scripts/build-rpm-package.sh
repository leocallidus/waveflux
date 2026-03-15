#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/build-rpm-package.sh [options]

Builds local RPM packages (*.rpm) using rpmbuild and a generated source tarball.

Options:
  --work-dir <dir>         Working directory (default: ./build-rpm)
  --output-dir <dir>       Output directory for built RPMs (default: ./dist/rpm)
  --package-name <name>    RPM package name (default: waveflux)
  --version <value>        Override upstream version from CMakeLists.txt (e.g. 1.2.3)
  --release <n>            RPM release value (default: 1)
  --arch <arch>            Build target arch for rpmbuild --target (e.g. x86_64)
  --packager <value>       Packager field in spec metadata
  --include-debug          Also copy debuginfo/debugsource RPMs to output
  --install-build-deps     Install missing build dependencies with dnf
  --skip-build-deps-check  Do not validate build dependencies before rpmbuild
  --no-clean               Keep previous working directory contents
  -h, --help               Show this help
EOF
}

print_fedora_build_requirements_hint() {
    cat >&2 <<'EOF'
Missing Fedora RPM build dependencies for full build mode.
Install them with:
  sudo dnf install -y \
    cmake gcc-c++ ninja-build pkgconf-pkg-config rpm-build \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kirigami-devel kf6-kcoreaddons-devel kf6-ki18n-devel \
    gstreamer1-devel gstreamer1-plugins-base-devel \
    taglib-devel
EOF
}

run_dnf_install() {
    if ! command -v dnf >/dev/null 2>&1; then
        echo "dnf is required to install build dependencies." >&2
        return 1
    fi

    if [[ "${EUID}" -eq 0 ]]; then
        dnf install -y "$@"
        return
    fi

    if ! command -v sudo >/dev/null 2>&1; then
        echo "sudo is not available; run as root or install dependencies manually." >&2
        return 1
    fi

    sudo dnf install -y "$@"
}

escape_sed() {
    printf '%s' "$1" | sed -e 's/[\/&]/\\&/g'
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

DEFAULT_WORK_DIR="${ROOT_DIR}/build-rpm"
DEFAULT_OUTPUT_DIR="${ROOT_DIR}/dist/rpm"
SPEC_TEMPLATE="${ROOT_DIR}/packaging/rpm/waveflux.spec.in"

WORK_DIR="${DEFAULT_WORK_DIR}"
OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
PACKAGE_NAME="waveflux"
VERSION_OVERRIDE=""
RPM_RELEASE="1"
ARCH_OVERRIDE=""
PACKAGER="WaveFlux Maintainers <noreply@example.com>"
INCLUDE_DEBUG=0
INSTALL_BUILD_DEPS=0
SKIP_BUILD_DEPS_CHECK=0
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
            RPM_RELEASE="${2:?Missing value for --release}"
            shift 2
            ;;
        --arch)
            ARCH_OVERRIDE="${2:?Missing value for --arch}"
            shift 2
            ;;
        --packager)
            PACKAGER="${2:?Missing value for --packager}"
            shift 2
            ;;
        --include-debug)
            INCLUDE_DEBUG=1
            shift
            ;;
        --install-build-deps)
            INSTALL_BUILD_DEPS=1
            shift
            ;;
        --skip-build-deps-check)
            SKIP_BUILD_DEPS_CHECK=1
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

for cmd in rpmbuild rpm tar sed awk find install; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        echo "Required command not found: ${cmd}" >&2
        exit 1
    fi
done

if [[ ! -f "${SPEC_TEMPLATE}" ]]; then
    echo "Spec template not found: ${SPEC_TEMPLATE}" >&2
    exit 1
fi

if [[ "${SKIP_BUILD_DEPS_CHECK}" -eq 0 ]]; then
    REQUIRED_BUILD_PACKAGES=(
        cmake
        gcc-c++
        ninja-build
        pkgconf-pkg-config
        rpm-build
        qt6-qtbase-devel
        qt6-qtdeclarative-devel
        kf6-kirigami-devel
        kf6-kcoreaddons-devel
        kf6-ki18n-devel
        gstreamer1-devel
        gstreamer1-plugins-base-devel
        taglib-devel
    )

    MISSING_BUILD_PACKAGES=()
    for pkg in "${REQUIRED_BUILD_PACKAGES[@]}"; do
        if ! rpm -q "${pkg}" >/dev/null 2>&1; then
            MISSING_BUILD_PACKAGES+=("${pkg}")
        fi
    done

    if ((${#MISSING_BUILD_PACKAGES[@]} > 0)); then
        if [[ "${INSTALL_BUILD_DEPS}" -eq 1 ]]; then
            echo "==> Installing missing packages: ${MISSING_BUILD_PACKAGES[*]}"
            run_dnf_install "${MISSING_BUILD_PACKAGES[@]}"
        else
            echo "Missing packages: ${MISSING_BUILD_PACKAGES[*]}" >&2
            print_fedora_build_requirements_hint
            exit 1
        fi
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

RPM_VERSION="${VERSION_OVERRIDE:-${PROJECT_VERSION}}"
SRC_BASENAME="${PACKAGE_NAME}-${RPM_VERSION}"
SRC_ARCHIVE="${SRC_BASENAME}.tar.gz"

RPM_TOPDIR="${WORK_DIR}/rpmbuild"
RPM_BUILD_DIR="${RPM_TOPDIR}/BUILD"
RPM_BUILDROOT_DIR="${RPM_TOPDIR}/BUILDROOT"
RPM_RPMS_DIR="${RPM_TOPDIR}/RPMS"
RPM_SOURCES_DIR="${RPM_TOPDIR}/SOURCES"
RPM_SPECS_DIR="${RPM_TOPDIR}/SPECS"
RPM_SRPMS_DIR="${RPM_TOPDIR}/SRPMS"

if [[ "${NO_CLEAN}" -eq 0 ]]; then
    rm -rf "${WORK_DIR}"
fi
mkdir -p "${WORK_DIR}" "${OUTPUT_DIR}"
mkdir -p \
    "${RPM_BUILD_DIR}" \
    "${RPM_BUILDROOT_DIR}" \
    "${RPM_RPMS_DIR}" \
    "${RPM_SOURCES_DIR}" \
    "${RPM_SPECS_DIR}" \
    "${RPM_SRPMS_DIR}"

SRC_ARCHIVE_PATH="${RPM_SOURCES_DIR}/${SRC_ARCHIVE}"
SPEC_PATH="${RPM_SPECS_DIR}/${PACKAGE_NAME}.spec"

echo "==> Creating source archive: ${SRC_ARCHIVE_PATH}"
tar -C "${ROOT_DIR}" \
    --exclude-vcs \
    --exclude='.idea' \
    --exclude='build' \
    --exclude='build-*' \
    --exclude='cmake-build-*' \
    --exclude='dist' \
    --exclude='.build-phase0' \
    --exclude='*.rpm' \
    --exclude='*.pkg.tar.zst' \
    --exclude='*.deb' \
    --exclude='*.AppImage' \
    --transform="s,^\.,${SRC_BASENAME}," \
    -czf "${SRC_ARCHIVE_PATH}" .

echo "==> Generating spec file: ${SPEC_PATH}"
sed \
    -e "s|@PACKAGE_NAME@|$(escape_sed "${PACKAGE_NAME}")|g" \
    -e "s|@VERSION@|$(escape_sed "${RPM_VERSION}")|g" \
    -e "s|@RELEASE@|$(escape_sed "${RPM_RELEASE}")|g" \
    -e "s|@SOURCE_ARCHIVE@|$(escape_sed "${SRC_ARCHIVE}")|g" \
    -e "s|@SOURCE_DIRNAME@|$(escape_sed "${SRC_BASENAME}")|g" \
    -e "s|@PACKAGER@|$(escape_sed "${PACKAGER}")|g" \
    "${SPEC_TEMPLATE}" > "${SPEC_PATH}"

RPMBUILD_ARGS=(
    -bb "${SPEC_PATH}"
    --define "_topdir ${RPM_TOPDIR}"
    --define "_builddir ${RPM_BUILD_DIR}"
    --define "_buildrootdir ${RPM_BUILDROOT_DIR}"
    --define "_rpmdir ${RPM_RPMS_DIR}"
    --define "_srcrpmdir ${RPM_SRPMS_DIR}"
    --define "_sourcedir ${RPM_SOURCES_DIR}"
)
if [[ -n "${ARCH_OVERRIDE}" ]]; then
    RPMBUILD_ARGS+=(--target "${ARCH_OVERRIDE}")
fi

echo "==> Building RPM package(s)"
rpmbuild "${RPMBUILD_ARGS[@]}"

mapfile -t BUILT_RPMS < <(find "${RPM_RPMS_DIR}" -type f -name '*.rpm' -print | sort)
if [[ "${#BUILT_RPMS[@]}" -eq 0 ]]; then
    echo "Package artifact was not produced." >&2
    exit 1
fi

COPIED_COUNT=0
for rpm_path in "${BUILT_RPMS[@]}"; do
    rpm_file="$(basename -- "${rpm_path}")"
    if [[ "${INCLUDE_DEBUG}" -eq 0 ]]; then
        if [[ "${rpm_file}" == *"-debuginfo-"* || "${rpm_file}" == *"-debugsource-"* ]]; then
            continue
        fi
    fi
    cp -f "${rpm_path}" "${OUTPUT_DIR}/"
    echo "Done: ${OUTPUT_DIR}/${rpm_file}"
    COPIED_COUNT=$((COPIED_COUNT + 1))
done

if [[ "${COPIED_COUNT}" -eq 0 ]]; then
    echo "No RPM files matched copy filters. Re-run with --include-debug if needed." >&2
    exit 1
fi
