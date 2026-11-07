#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Clean Utility Script
# ==============================================================================
# Usage:
#   ./scripts/linux/clean.sh [OPTIONS]
#
# Options:
#   -q, --qt       Remove only Desktop Qt build (build/qt-linux)
#   -c, --core     Remove only Zero-Qt Core build (build/core-linux)
#   -a, --all      Remove all build directories (build/*) [default]
#   -d, --dry-run  Show directories that would be removed without deleting
#   -h, --help     Show this help message
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

CLEAN_QT=false
CLEAN_CORE=false
CLEAN_ALL=true
DRY_RUN=false

show_help() {
    cat << EOF
${BOLD}Git Flux — Linux Build Cleaning Script${NC}

${CYAN}Usage:${NC}
  $0 [OPTIONS]

${CYAN}Options:${NC}
  -a, --all      Remove the entire contents of the build/ directory [default]
  -q, --qt       Remove only Qt Quick Desktop build (build/qt-linux)
  -c, --core     Remove only Zero-Qt Core build (build/core-linux)
  -d, --dry-run  Show what would be removed without deleting anything
  -h, --help     Show this help message

${CYAN}Examples:${NC}
  $0             # Clean all builds
  $0 --qt        # Clean only build/qt-linux
  $0 --core      # Clean only build/core-linux
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -q|--qt)
            CLEAN_QT=true
            CLEAN_ALL=false
            shift
            ;;
        -c|--core)
            CLEAN_CORE=true
            CLEAN_ALL=false
            shift
            ;;
        -a|--all)
            CLEAN_ALL=true
            shift
            ;;
        -d|--dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}Unrecognized option: $1${NC}" >&2
            echo "Use $0 --help to display help." >&2
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}"

remove_path() {
    local target_path="$1"
    if [[ -e "${target_path}" ]]; then
        if [[ "${DRY_RUN}" == true ]]; then
            echo -e "${CYAN}[DRY-RUN] Would remove: ${BOLD}${target_path}${NC}"
        else
            echo -e "${YELLOW}🗑️  Removing: ${BOLD}${target_path}${NC}..."
            rm -rf "${target_path}"
            echo -e "${GREEN}✔ Removed: ${target_path}${NC}"
        fi
    else
        echo -e "${CYAN}ℹ️  Path does not exist (already clean): ${target_path}${NC}"
    fi
}

echo -e "${BOLD}▶ Starting build directory cleanup...${NC}"

if [[ "${CLEAN_ALL}" == true ]]; then
    remove_path "build/qt-linux"
    remove_path "build/core-linux"
    remove_path "build/qt-msvc"
    remove_path "build/core-msvc"
elif [[ "${CLEAN_QT}" == true ]]; then
    remove_path "build/qt-linux"
elif [[ "${CLEAN_CORE}" == true ]]; then
    remove_path "build/core-linux"
fi

echo -e "\n${GREEN}${BOLD}Cleanup operation completed!${NC}"
