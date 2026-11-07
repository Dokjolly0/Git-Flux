#!/usr/bin/env bash
# ==============================================================================
# Git Flux — macOS Packaging Script (DMG & ZIP)
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/qt-macos"
PACKAGE_DIR="${ROOT_DIR}/build/packages"
VERSION="0.1.0"

mkdir -p "${PACKAGE_DIR}"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 Git Flux — Building macOS Package (.dmg & .zip)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 1. Build target
if [ ! -d "${BUILD_DIR}" ]; then
    echo "⚙️ Configuring macOS build..."
    cmake -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DGF_BUILD_DESKTOP=ON \
        -DGF_BUILD_TESTING=OFF \
        -DGF_BUILD_BENCHMARKS=OFF
fi

echo "🔨 Building git_flux target..."
cmake --build "${BUILD_DIR}" --target git_flux

APP_PATH="${BUILD_DIR}/git_flux.app"
if [ ! -d "${APP_PATH}" ]; then
    # Create bundle structure if built as unix binary
    echo "📦 Creating macOS App Bundle..."
    APP_PATH="${BUILD_DIR}/GitFlux.app"
    mkdir -p "${APP_PATH}/Contents/MacOS"
    mkdir -p "${APP_PATH}/Contents/Resources"
    cp "${BUILD_DIR}/git_flux" "${APP_PATH}/Contents/MacOS/git_flux"
    chmod +x "${APP_PATH}/Contents/MacOS/git_flux"
    cp "${ROOT_DIR}/assets/logo.png" "${APP_PATH}/Contents/Resources/icon.png"

    cat << EOF > "${APP_PATH}/Contents/Info.plist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>git_flux</string>
    <key>CFBundleIdentifier</key>
    <string>com.gitflux.desktop</string>
    <key>CFBundleName</key>
    <string>Git Flux</string>
    <key>CFBundleIconFile</key>
    <string>icon.png</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
fi

# 2. Run macdeployqt
echo "📦 Running macdeployqt..."
if command -v macdeployqt &>/dev/null; then
    macdeployqt "${APP_PATH}" -qmldir="${ROOT_DIR}/src/desktop/ui/qml" -dmg
    mv "${BUILD_DIR}"/*.dmg "${PACKAGE_DIR}/git-flux-${VERSION}-macos.dmg" 2>/dev/null || true
fi

# 3. Also package ZIP of the App Bundle
ZIP_OUTPUT="${PACKAGE_DIR}/git-flux-${VERSION}-macos.zip"
echo "📦 Creating ZIP: ${ZIP_OUTPUT}..."
(cd "${BUILD_DIR}" && zip -r -q "${ZIP_OUTPUT}" "$(basename "${APP_PATH}")")

echo "✔ macOS packages created in ${PACKAGE_DIR}:"
ls -lh "${PACKAGE_DIR}"/*macos* 2>/dev/null || true
