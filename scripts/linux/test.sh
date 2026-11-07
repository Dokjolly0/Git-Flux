#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Test Runner Script
# ==============================================================================
# Usage:
#   ./scripts/linux/test.sh [OPTIONS]
#
# Options:
#   -q, --qt             Run Desktop Qt preset tests (default: linux-qt-release)
#   -c, --core           Run Zero-Qt Core preset tests (linux-core-release)
#   -a, --all            Run tests for both presets (core + desktop)
#   -f, --filter REGEX   Filter tests to run using a regular expression (-R)
#   -e, --exclude REGEX  Exclude tests matching regular expression (-E)
#   -s, --smoke          Run only the headless application startup smoke test
#   -v, --verbose        Print detailed output for each individual test
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

PRESET="linux-qt-release"
TEST_ALL=false
SMOKE_ONLY=false
INCLUDE_REGEX=""
EXCLUDE_REGEX=""
VERBOSE=false

show_help() {
    cat << EOF
${BOLD}Git Flux — Linux Test Runner Script${NC}

${CYAN}Usage:${NC}
  $0 [OPTIONS]

${CYAN}Options:${NC}
  -q, --qt              Run Desktop Qt Quick 6 tests [default] (${BOLD}linux-qt-release${NC})
  -c, --core            Run Zero-Qt Core C++23 tests (${BOLD}linux-core-release${NC})
  -a, --all             Run all tests across both presets
  -f, --filter <REGEX>  Filter tests by name or suite (e.g. -f unit, -f viewmodels, -f integration)
  -e, --exclude <REGEX> Exclude tests matching the pattern
  -s, --smoke           Run only headless startup smoke test (./build/qt-linux/git_flux --test-startup)
  -v, --verbose         Detailed output with full test logs
  -h, --help            Show this help message

${CYAN}Examples:${NC}
  $0                        # Run all desktop tests (unit, viewmodels, integration)
  $0 --core                 # Run all C++23 core tests
  $0 --all                  # Run the entire test suite on both presets
  $0 -f viewmodels          # Run only the viewmodels test suite
  $0 --smoke                # Verify Git Flux headless startup
EOF
}

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
            TEST_ALL=true
            shift
            ;;
        -f|--filter)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Error: --filter requires a regex pattern.${NC}" >&2
                exit 1
            fi
            INCLUDE_REGEX="$2"
            shift 2
            ;;
        -e|--exclude)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Error: --exclude requires a regex pattern.${NC}" >&2
                exit 1
            fi
            EXCLUDE_REGEX="$2"
            shift 2
            ;;
        -s|--smoke)
            SMOKE_ONLY=true
            shift
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

run_smoke_test() {
    local bin_path="build/qt-linux/git_flux"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}▶ Running Headless Smoke Test: ${YELLOW}${bin_path}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ ! -x "${bin_path}" ]]; then
        echo -e "${YELLOW}⚠️ Executable ${bin_path} not found. Starting build...${NC}"
        cmake --build --preset linux-qt-release --target git_flux
    fi

    local start_time
    start_time=$(date +%s%N)

    "${bin_path}" --test-startup

    local end_time
    end_time=$(date +%s%N)
    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))

    echo -e "${GREEN}✔ Smoke Test passed successfully in ${elapsed_ms}ms!${NC}\n"
}

run_preset_tests() {
    local target_preset="$1"
    local build_dir=""

    if [[ "${target_preset}" == "linux-qt-release" ]]; then
        build_dir="build/qt-linux"
    elif [[ "${target_preset}" == "linux-core-release" ]]; then
        build_dir="build/core-linux"
    fi

    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}▶ Running tests for preset: ${YELLOW}${target_preset}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ ! -d "${build_dir}" ]]; then
        echo -e "${YELLOW}⚠️ Build directory not found. Running preliminary build...${NC}"
        cmake --build --preset "${target_preset}"
    fi

    local ctest_cmd=("ctest" "--preset" "${target_preset}" "--output-on-failure")

    if [[ -n "${INCLUDE_REGEX}" ]]; then
        echo -e "${CYAN}🔍 Inclusion filter: ${BOLD}${INCLUDE_REGEX}${NC}"
        ctest_cmd+=("-R" "${INCLUDE_REGEX}")
    fi

    if [[ -n "${EXCLUDE_REGEX}" ]]; then
        echo -e "${CYAN}🚫 Exclusion filter: ${BOLD}${EXCLUDE_REGEX}${NC}"
        ctest_cmd+=("-E" "${EXCLUDE_REGEX}")
    fi

    if [[ "${VERBOSE}" == true ]]; then
        ctest_cmd+=("--verbose")
    fi

    local start_time
    start_time=$(date +%s)

    echo -e "${CYAN}🧪 Running: ${ctest_cmd[*]}${NC}"
    "${ctest_cmd[@]}"

    local end_time
    end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo -e "${GREEN}✔ Tests for '${target_preset}' completed successfully in ${elapsed}s!${NC}\n"
}

if [[ "${SMOKE_ONLY}" == true ]]; then
    run_smoke_test
    exit 0
fi

START_ALL=$(date +%s)

if [[ "${TEST_ALL}" == true ]]; then
    run_preset_tests "linux-core-release"
    run_preset_tests "linux-qt-release"
else
    run_preset_tests "${PRESET}"
fi

END_ALL=$(date +%s)
TOTAL_ELAPSED=$((END_ALL - START_ALL))

echo -e "${GREEN}${BOLD}============================================================${NC}"
echo -e "${GREEN}${BOLD}  ALL TESTS PASSED SUCCESSFULLY (${TOTAL_ELAPSED}s)${NC}"
echo -e "${GREEN}${BOLD}============================================================${NC}"
