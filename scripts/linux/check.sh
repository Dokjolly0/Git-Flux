#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Full Health Check (Pre-Commit / CI Local Verification)
# ==============================================================================
# Usage:
#   ./scripts/linux/check.sh
#
# Runs in sequence:
#   1. Zero-Qt Core C++23 build (linux-core-release)
#   2. Core Unit & Integration Tests (ctest core)
#   3. Desktop Qt Quick 6 build (linux-qt-release)
#   4. Desktop Unit, MVVM & Integration Tests (ctest qt)
#   5. Headless Application Startup Smoke Test (git_flux --test-startup)
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

TOTAL_START=$(date +%s)
STEP_NAMES=()
STEP_DURATIONS=()
STEP_STATUSES=()

run_step() {
    local name="$1"
    shift
    local cmd=("$@")

    echo -e "\n${BLUE}============================================================${NC}"
    echo -e "${BOLD}${CYAN}▶ STEP: ${name}${NC}"
    echo -e "${BLUE}============================================================${NC}"

    local start_time
    start_time=$(date +%s)
    local status="PASS"

    if "${cmd[@]}"; then
        status="PASS"
        echo -e "${GREEN}✔ ${name} completed successfully!${NC}"
    else
        status="FAIL"
        echo -e "${RED}✖ ${name} FAILED!${NC}" >&2
    fi

    local end_time
    end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    STEP_NAMES+=("${name}")
    STEP_DURATIONS+=("${elapsed}s")
    STEP_STATUSES+=("${status}")

    if [[ "${status}" == "FAIL" ]]; then
        print_summary
        exit 1
    fi
}

print_summary() {
    local total_end
    total_end=$(date +%s)
    local total_elapsed=$((total_end - TOTAL_START))

    echo -e "\n${BOLD}============================================================${NC}"
    echo -e "${BOLD}              GIT FLUX HEALTH CHECK SUMMARY                 ${NC}"
    echo -e "${BOLD}============================================================${NC}"

    for i in "${!STEP_NAMES[@]}"; do
        local color="${GREEN}"
        if [[ "${STEP_STATUSES[$i]}" == "FAIL" ]]; then
            color="${RED}"
        fi
        printf "  %-42s [%b%-4s%b] (%s)\n" "${STEP_NAMES[$i]}" "${color}" "${STEP_STATUSES[$i]}" "${NC}" "${STEP_DURATIONS[$i]}"
    done

    echo -e "${BOLD}------------------------------------------------------------${NC}"
    echo -e "  Total time elapsed: ${BOLD}${total_elapsed}s${NC}"
    echo -e "${BOLD}============================================================${NC}\n"
}

cd "${ROOT_DIR}"

run_step "1. Build Zero-Qt Core (linux-core-release)" "${SCRIPT_DIR}/build.sh" --core
run_step "2. Test Suite Zero-Qt Core" "${SCRIPT_DIR}/test.sh" --core
run_step "3. Build Desktop Qt Quick (linux-qt-release)" "${SCRIPT_DIR}/build.sh" --qt
run_step "4. Test Suite Desktop & MVVM" "${SCRIPT_DIR}/test.sh" --qt
run_step "5. Headless Smoke Test (git_flux)" "${SCRIPT_DIR}/test.sh" --smoke

print_summary
echo -e "${GREEN}${BOLD}🎉 ALL CHECKS PASSED SUCCESSFULLY!${NC}\n"
