#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Test Runner Script
# ==============================================================================
# Utilizzo:
#   ./scripts/linux/test.sh [OPZIONI]
#
# Opzioni:
#   -q, --qt             Esegue i test del preset Desktop Qt (default: linux-qt-release)
#   -c, --core           Esegue i test del preset Zero-Qt Core (linux-core-release)
#   -a, --all            Esegue i test di entrambi i preset (core + desktop)
#   -f, --filter REGEX   Filtra i test da eseguire tramite espressione regolare (-R)
#   -e, --exclude REGEX  Esclude i test che matchano l'espressione regolare (-E)
#   -s, --smoke          Esegue solo lo smoke test headless di avvio dell'applicazione
#   -v, --verbose        Stampa output dettagliato di ogni singolo test
#   -h, --help           Mostra questa guida
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
${BOLD}Git Flux — Script di Esecuzione Test Linux${NC}

${CYAN}Utilizzo:${NC}
  $0 [OPZIONI]

${CYAN}Opzioni:${NC}
  -q, --qt              Esegue i test Desktop Qt Quick 6 [default] (${BOLD}linux-qt-release${NC})
  -c, --core            Esegue i test Zero-Qt Core C++23 (${BOLD}linux-core-release${NC})
  -a, --all             Esegue tutti i test di entrambi i preset
  -f, --filter <REGEX>  Filtra i test per nome o suite (es. -f unit, -f viewmodels, -f integration)
  -e, --exclude <REGEX> Esclude i test che matchano il pattern
  -s, --smoke           Esegue unicamente lo smoke test headless di avvio (./build/qt-linux/git_flux --test-startup)
  -v, --verbose         Output dettagliato con log completi dei test
  -h, --help            Mostra questa guida

${CYAN}Esempi:${NC}
  $0                        # Esegue tutti i test desktop (unit, viewmodels, integration)
  $0 --core                 # Esegue tutti i test del core C++23
  $0 --all                  # Esegue l'intera suite su entrambi i preset
  $0 -f viewmodels          # Esegue unicamente la test suite viewmodels
  $0 --smoke                # Verifica lo startup headless di Git Flux
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
                echo -e "${RED}Errore: --filter richiede un pattern regex.${NC}" >&2
                exit 1
            fi
            INCLUDE_REGEX="$2"
            shift 2
            ;;
        -e|--exclude)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Errore: --exclude richiede un pattern regex.${NC}" >&2
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
            echo -e "${RED}Opzione non riconosciuta: $1${NC}" >&2
            echo "Usa $0 --help per visualizzare la guida." >&2
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}"

run_smoke_test() {
    local bin_path="build/qt-linux/git_flux"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}${CYAN}▶ Esecuzione Headless Smoke Test: ${YELLOW}${bin_path}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ ! -x "${bin_path}" ]]; then
        echo -e "${YELLOW}⚠️ Eseguibile ${bin_path} non trovato. Avvio compilazione...${NC}"
        cmake --build --preset linux-qt-release --target git_flux
    fi

    local start_time
    start_time=$(date +%s%N)

    "${bin_path}" --test-startup

    local end_time
    end_time=$(date +%s%N)
    local elapsed_ms=$(( (end_time - start_time) / 1000000 ))

    echo -e "${GREEN}✔ Smoke Test superato con successo in ${elapsed_ms}ms!${NC}\n"
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
    echo -e "${BOLD}${CYAN}▶ Esecuzione test per preset: ${YELLOW}${target_preset}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ ! -d "${build_dir}" ]]; then
        echo -e "${YELLOW}⚠️ Directory di build non trovata. Esecuzione build preliminare...${NC}"
        cmake --build --preset "${target_preset}"
    fi

    local ctest_cmd=("ctest" "--preset" "${target_preset}" "--output-on-failure")

    if [[ -n "${INCLUDE_REGEX}" ]]; then
        echo -e "${CYAN}🔍 Filtro inclusione: ${BOLD}${INCLUDE_REGEX}${NC}"
        ctest_cmd+=("-R" "${INCLUDE_REGEX}")
    fi

    if [[ -n "${EXCLUDE_REGEX}" ]]; then
        echo -e "${CYAN}🚫 Filtro esclusione: ${BOLD}${EXCLUDE_REGEX}${NC}"
        ctest_cmd+=("-E" "${EXCLUDE_REGEX}")
    fi

    if [[ "${VERBOSE}" == true ]]; then
        ctest_cmd+=("--verbose")
    fi

    local start_time
    start_time=$(date +%s)

    echo -e "${CYAN}🧪 Esecuzione: ${ctest_cmd[*]}${NC}"
    "${ctest_cmd[@]}"

    local end_time
    end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo -e "${GREEN}✔ Test per '${target_preset}' completati con successo in ${elapsed}s!${NC}\n"
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
echo -e "${GREEN}${BOLD}  TUTTI I TEST SONO PASSATI CON SUCCESSO (${TOTAL_ELAPSED}s)${NC}"
echo -e "${GREEN}${BOLD}============================================================${NC}"
