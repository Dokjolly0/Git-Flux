#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Universal Packaging Script (Debian, Ubuntu, Fedora, Arch)
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "🚀 Git Flux — Building All Linux Distribution Packages"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 1. Debian & Ubuntu (.deb)
bash "${SCRIPT_DIR}/package_deb.sh"

# 2. Arch Linux (.pkg.tar.zst)
bash "${SCRIPT_DIR}/package_arch.sh"

# 3. Generic Linux Tarball (.tar.gz)
bash "${SCRIPT_DIR}/package_tarball.sh"

# 4. Fedora / RHEL (.rpm)
if command -v rpmbuild &>/dev/null || command -v rpm &>/dev/null; then
    bash "${SCRIPT_DIR}/package_rpm.sh"
else
    echo "ℹ️  Skipping RPM build (rpmbuild not installed locally). Will be built in GitHub Actions CI."
fi

echo ""
echo "============================================================"
echo "📦 GENERATED PACKAGES SUMMARY:"
echo "============================================================"
ls -lh "${ROOT_DIR}/build/packages"
echo "============================================================"
