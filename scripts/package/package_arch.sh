#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/qt-linux"
PACKAGE_DIR="${ROOT_DIR}/build/packages"
VERSION="0.1.0"
PKGREL="1"
ARCH="x86_64"

mkdir -p "${PACKAGE_DIR}"
STAGE_DIR=$(mktemp -d)
trap 'rm -rf "${STAGE_DIR}"' EXIT

echo "📦 Packaging for Arch Linux (git-flux-${VERSION}-${PKGREL}-${ARCH}.pkg.tar.zst)..."

if [ ! -f "${BUILD_DIR}/git_flux" ]; then
    echo "🔨 Building git_flux target first..."
    cmake --build "${BUILD_DIR}" --target git_flux
fi

# Install into staging directory
DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}" --prefix /usr > /dev/null

# Compute installed size in bytes
SIZE=$(du -sb "${STAGE_DIR}" | awk '{print $1}')
BUILD_DATE=$(date +%s)

# Create .PKGINFO
cat << EOF > "${STAGE_DIR}/.PKGINFO"
pkgname = git-flux
pkgbase = git-flux
pkgver = ${VERSION}-${PKGREL}
pkgdesc = Git Flux — Modern local-first desktop Git client
url = https://github.com/dokjolly/GitFlux
builddate = ${BUILD_DATE}
packager = GitFlux Team <support@gitflux.org>
size = ${SIZE}
arch = ${ARCH}
license = MIT
depend = qt6-base
depend = qt6-declarative
depend = git
EOF

# Create .BUILDINFO
cat << EOF > "${STAGE_DIR}/.BUILDINFO"
format = 2
pkgname = git-flux
pkgver = ${VERSION}-${PKGREL}
pkgarch = ${ARCH}
pkgbuild_sha256sum = 0
builddate = ${BUILD_DATE}
packager = GitFlux Team <support@gitflux.org>
buildenv = (check !sign)
installed = qt6-base qt6-declarative git
EOF

# Create tar.zst archive with Arch package format
OUTPUT_PKG="${PACKAGE_DIR}/git-flux-${VERSION}-${PKGREL}-${ARCH}.pkg.tar.zst"
(cd "${STAGE_DIR}" && tar --zstd -cf "${OUTPUT_PKG}" .PKGINFO .BUILDINFO usr)

echo "✔ Arch Linux package created: ${OUTPUT_PKG}"
ls -lh "${OUTPUT_PKG}"
