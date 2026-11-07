#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/qt-linux"
PACKAGE_DIR="${ROOT_DIR}/build/packages"

mkdir -p "${PACKAGE_DIR}"

echo "📦 Packaging for Debian / Ubuntu (.deb)..."

if [ ! -f "${BUILD_DIR}/git_flux" ]; then
    echo "🔨 Building git_flux target first..."
    cmake --build "${BUILD_DIR}" --target git_flux
fi

(cd "${BUILD_DIR}" && cpack -G DEB)

# Move and rename generated .deb to build/packages
mv "${BUILD_DIR}"/git-flux-*.deb "${PACKAGE_DIR}/"

echo "✔ Debian package created in ${PACKAGE_DIR}:"
ls -lh "${PACKAGE_DIR}"/*.deb
