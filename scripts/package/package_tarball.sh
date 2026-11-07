#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/qt-linux"
PACKAGE_DIR="${ROOT_DIR}/build/packages"
VERSION="0.1.0"
ARCH="x86_64"

mkdir -p "${PACKAGE_DIR}"

echo "📦 Packaging Generic Portable Linux Tarball (.tar.gz)..."

if [ ! -f "${BUILD_DIR}/git_flux" ]; then
    echo "🔨 Building git_flux target first..."
    cmake --build "${BUILD_DIR}" --target git_flux
fi

(cd "${BUILD_DIR}" && cpack -G TGZ)

OUTPUT_FILE="${PACKAGE_DIR}/git-flux-${VERSION}-linux-${ARCH}.tar.gz"
mv "${BUILD_DIR}"/git-flux-*.tar.gz "${OUTPUT_FILE}"

echo "✔ Tarball created: ${OUTPUT_FILE}"
ls -lh "${OUTPUT_FILE}"
