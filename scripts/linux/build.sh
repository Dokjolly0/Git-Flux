#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Build Utility Script
# ==============================================================================
# Usage:
#   ./scripts/linux/build.sh [OPTIONS]
#
# Options:
#   -q, --qt            Build Desktop Qt Quick 6 preset (default: linux-qt-release)
#   -c, --core          Build Zero-Qt Core C++23 preset (linux-core-release)
#   -a, --all           Build both presets (core + desktop)
#   -C, --clean         Clean build directory before building
#   -g, --configure     Force cmake configuration step before build
#   -t, --target NAME   Build only the specified target (e.g. git_flux, gf_unit_tests)
#   -j, --jobs NUM      Number of parallel jobs (default: nproc)
#   -v, --verbose       Verbose compiler output
#   -h, --help          Show this help message
# ==============================================================================

set -euo pipefail

# Terminal colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Repository root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Default parameters
PRESET="linux-qt-release"
BUILD_ALL=false
CLEAN_BUILD=false
FORCE_CONFIGURE=false
SPECIFIC_TARGET=""
JOBS=$(nproc 2>/dev/null || echo 4)
VERBOSE=false

show_help() {
    cat << EOF
${BOLD}Git Flux — Linux Build Script${NC}

${CYAN}Usage:${NC}
  $0 [OPTIONS]

${CYAN}Options:${NC}
  -q, --qt             Build Desktop Qt Quick 6 preset [default] (${BOLD}linux-qt-release${NC})
  -c, --core           Build Zero-Qt Core C++23 preset (${BOLD}linux-core-release${NC})
  -a, --all            Build both Core and Desktop in sequence
  -C, --clean          Remove build directory before building
  -g, --configure      Force CMake reconfiguration before build
  -t, --target <NAME>  Build only the specified target (e.g. git_flux)
  -j, --jobs <NUM>     Number of parallel build jobs (default: ${JOBS})
  -v, --verbose        Verbose mode (print compiler command lines)
  -h, --help           Show this help message

${CYAN}Examples:${NC}
  $0                         # Full build of standard desktop target
  $0 --core                  # Build pure C++23 core (Zero-Qt)
  $0 --all                   # Full build of both presets
  $0 --clean --qt            # Clean build of desktop
  $0 -t git_flux             # Build only the main executable
  $0 -t gf_viewmodels_tests  # Build only the MVVM test executable
EOF
}

# Argument parsing
while [[ $# -gt 0 ]]; do
    case "$1" in
        -q|--qt)
            PRESET="linux-qt-release"
            shift
            ;;
        -c|--core)
            PRESET="linux-core-release"
            shift
            ;;
        -a|--all)
            BUILD_ALL=true
            shift
            ;;
        -C|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -g|--configure)
            FORCE_CONFIGURE=true
            shift
            ;;
        -t|--target)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Error: --target requires a target name.${NC}" >&2
                exit 1
            fi
            SPECIFIC_TARGET="$2"
            shift 2
            ;;
        -j|--jobs)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Error: --jobs requires a number.${NC}" >&2
                exit 1
            fi
            JOBS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
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

build_preset() {
    local target_preset="$1"
    local build_dir=""

    if [[ "${target_preset}" == "linux-qt-release" ]]; then
        build_dir="build/qt-linux"
    elif [[ "${target_preset}" == "linux-core-release" ]]; then
        build_dir="build/core-linux"
    fi

    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}▶ Starting build for preset: ${YELLOW}${target_preset}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ "${CLEAN_BUILD}" == true && -n "${build_dir}" && -d "${build_dir}" ]]; then
        echo -e "${YELLOW}🧹 Cleaning build directory: ${build_dir}...${NC}"
        rm -rf "${build_dir}"
    fi

    if [[ "${FORCE_CONFIGURE}" == true || ! -d "${build_dir}" ]]; then
        echo -e "${CYAN}⚙️  Configuring CMake with preset '${target_preset}'...${NC}"
        cmake --preset "${target_preset}"
    fi

    local build_cmd=("cmake" "--build" "--preset" "${target_preset}" "-j" "${JOBS}")

    if [[ -n "${SPECIFIC_TARGET}" ]]; then
        echo -e "${CYAN}🎯 Building specific target: ${BOLD}${SPECIFIC_TARGET}${NC}"
        build_cmd+=("--target" "${SPECIFIC_TARGET}")
    fi

    if [[ "${VERBOSE}" == true ]]; then
        build_cmd+=("--verbose")
    fi

    local start_time
    start_time=$(date +%s)

    echo -e "${CYAN}🔨 Running: ${build_cmd[*]}${NC}"
    "${build_cmd[@]}"

    local end_time
    end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo -e "${GREEN}✔ Build for '${target_preset}' completed successfully in ${elapsed}s!${NC}\n"
}

START_ALL=$(date +%s)

if [[ "${BUILD_ALL}" == true ]]; then
    build_preset "linux-core-release"
    build_preset "linux-qt-release"
else
    build_preset "${PRESET}"
fi

END_ALL=$(date +%s)
TOTAL_ELAPSED=$((END_ALL - START_ALL))

echo -e "${GREEN}${BOLD}============================================================${NC}"
echo -e "${GREEN}${BOLD}  ALL BUILD OPERATIONS COMPLETED (${TOTAL_ELAPSED}s)${NC}"
echo -e "${GREEN}${BOLD}============================================================${NC}"
