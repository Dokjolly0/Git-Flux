#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/qt-linux"
PACKAGE_DIR="${ROOT_DIR}/build/packages"

mkdir -p "${PACKAGE_DIR}"

echo "📦 Packaging for Fedora / RHEL (.rpm)..."

if ! command -v rpmbuild &>/dev/null && ! command -v rpm &>/dev/null; then
    echo "⚠️  'rpmbuild' not found on system. On Ubuntu/Debian, install with: sudo apt install -y rpm"
    echo "    On Fedora, install with: sudo dnf install -y rpm-build"
    exit 1
fi

if [ ! -f "${BUILD_DIR}/git_flux" ]; then
    echo "🔨 Building git_flux target first..."
    cmake --build "${BUILD_DIR}" --target git_flux
fi

(cd "${BUILD_DIR}" && cpack -G RPM)

# Move generated .rpm to build/packages
mv "${BUILD_DIR}"/git-flux-*.rpm "${PACKAGE_DIR}/"

echo "✔ RPM package created in ${PACKAGE_DIR}:"
ls -lh "${PACKAGE_DIR}"/*.rpm
