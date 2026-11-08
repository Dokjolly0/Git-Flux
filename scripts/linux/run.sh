#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Application Runner Script
# ==============================================================================
# Usage:
#   ./scripts/linux/run.sh [OPTIONS] [-- APP_ARGS...]
#
# Options:
#   -b, --build          Build application before launching
#   -s, --smoke          Run in headless smoke test mode (--test-startup)
#   -d, --gdb            Launch under GDB debugger
#   -v, --valgrind       Launch under Valgrind (memcheck)
#   -h, --help           Show this help message
# ==============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

BIN_PATH="${ROOT_DIR}/build/qt-linux/git_flux"
REBUILD_FIRST=false
USE_GDB=false
USE_VALGRIND=false
APP_ARGS=()

show_help() {
    cat << EOF
${BOLD}Git Flux — Linux Application Runner Script${NC}

${CYAN}Usage:${NC}
  $0 [OPTIONS] [-- ARGS_FOR_GIT_FLUX]

${CYAN}Options:${NC}
  -b, --build      Automatically build git_flux target before launching
  -s, --smoke      Run headless startup smoke test (--test-startup)
  -d, --gdb        Run application inside GDB debugger
  -v, --valgrind   Run application under Valgrind Memcheck
  -h, --help       Show this help message

${CYAN}Examples:${NC}
  $0                         # Launch Git Flux desktop graphical interface
  $0 --build                 # Build and then launch application
  $0 --smoke                 # Run fast headless smoke test
  $0 --gdb                   # Interactive debugging with GDB
  $0 -- --help               # Pass flags and options directly to Git Flux
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build)
            REBUILD_FIRST=true
            shift
            ;;
        -s|--smoke)
            APP_ARGS+=("--test-startup")
            shift
            ;;
        -d|--gdb)
            USE_GDB=true
            shift
            ;;
        -v|--valgrind)
            USE_VALGRIND=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        --)
            shift
            APP_ARGS+=("$@")
            break
            ;;
        *)
            APP_ARGS+=("$1")
            shift
            ;;
    esac
done

cd "${ROOT_DIR}"

# Clean up legacy duplicate .desktop files from previous versions
rm -f ~/.local/share/applications/git_flux.desktop ~/.local/share/applications/GitFlux.desktop 2>/dev/null || true

# Register Linux desktop icon and canonical .desktop file (only if not already installed system-wide)
if [[ -f "${ROOT_DIR}/assets/logo.png" ]]; then
    mkdir -p ~/.local/share/icons/hicolor/512x512/apps
    cp -u "${ROOT_DIR}/assets/logo.png" ~/.local/share/icons/hicolor/512x512/apps/git-flux.png 2>/dev/null || true

    if [[ ! -f "/usr/share/applications/git-flux.desktop" ]]; then
        mkdir -p ~/.local/share/applications
        cat << EOF > ~/.local/share/applications/git-flux.desktop
[Desktop Entry]
Type=Application
Name=Git Flux
GenericName=Git Client
Comment=Git Flux — Modern local-first desktop Git client
Exec=${BIN_PATH} %U
Icon=${ROOT_DIR}/assets/logo.png
Terminal=false
Categories=Development;RevisionControl;
StartupWMClass=GitFlux
MimeType=x-scheme-handler/gitflux;
EOF
        chmod +x ~/.local/share/applications/git-flux.desktop 2>/dev/null || true
    fi

    update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
fi

if [[ "${REBUILD_FIRST}" == true || ! -x "${BIN_PATH}" ]]; then
    echo -e "${CYAN}🔨 Building target git_flux...${NC}"
    "${SCRIPT_DIR}/build.sh" -t git_flux
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}${CYAN}▶ Launching Git Flux Desktop: ${YELLOW}${BIN_PATH}${NC}"
if [[ ${#APP_ARGS[@]} -gt 0 ]]; then
    echo -e "${CYAN}  Arguments: ${BOLD}${APP_ARGS[*]}${NC}"
fi
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if [[ "${USE_GDB}" == true ]]; then
    echo -e "${YELLOW}🐛 Launching under GDB...${NC}"
    gdb --args "${BIN_PATH}" "${APP_ARGS[@]}"
elif [[ "${USE_VALGRIND}" == true ]]; then
    echo -e "${YELLOW}🔍 Launching under Valgrind...${NC}"
    valgrind --leak-check=full --track-origins=yes "${BIN_PATH}" "${APP_ARGS[@]}"
else
    "${BIN_PATH}" "${APP_ARGS[@]}"
fi
