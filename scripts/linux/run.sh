#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Application Runner Script
# ==============================================================================
# Utilizzo:
#   ./scripts/linux/run.sh [OPZIONI] [-- ARGOMENTI_APP...]
#
# Opzioni:
#   -b, --build          Compila l'applicazione prima di avviarla
#   -s, --smoke          Esegue in modalità headless smoke test (--test-startup)
#   -d, --gdb            Avvia sotto il debugger GDB
#   -v, --valgrind       Avvia sotto Valgrind (memcheck)
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

BIN_PATH="${ROOT_DIR}/build/qt-linux/git_flux"
REBUILD_FIRST=false
USE_GDB=false
USE_VALGRIND=false
APP_ARGS=()

show_help() {
    cat << EOF
${BOLD}Git Flux — Script di Avvio Applicazione Linux${NC}

${CYAN}Utilizzo:${NC}
  $0 [OPZIONI] [-- ARGOMENTI_PER_GIT_FLUX]

${CYAN}Opzioni:${NC}
  -b, --build      Compila automaticamente il target git_flux prima di avviarlo
  -s, --smoke      Esegue lo smoke test headless di avvio (--test-startup)
  -d, --gdb        Esegue l'applicazione all'interno del debugger GDB
  -v, --valgrind   Esegue l'applicazione sotto Valgrind Memcheck
  -h, --help       Mostra questa guida

${CYAN}Esempi:${NC}
  $0                         # Avvia l'interfaccia grafica desktop di Git Flux
  $0 --build                 # Compila e poi avvia l'applicazione
  $0 --smoke                 # Esegue lo smoke test headless rapido
  $0 --gdb                   # Debug interattivo con GDB
  $0 -- --help               # Passa flag e opzioni direttamente a Git Flux
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

if [[ "${REBUILD_FIRST}" == true || ! -x "${BIN_PATH}" ]]; then
    echo -e "${CYAN}🔨 Compilazione target git_flux...${NC}"
    "${SCRIPT_DIR}/build.sh" -t git_flux
fi

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BOLD}${CYAN}▶ Avvio Git Flux Desktop: ${YELLOW}${BIN_PATH}${NC}"
if [[ ${#APP_ARGS[@]} -gt 0 ]]; then
    echo -e "${CYAN}  Argomenti: ${BOLD}${APP_ARGS[*]}${NC}"
fi
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

if [[ "${USE_GDB}" == true ]]; then
    echo -e "${YELLOW}🐛 Avvio sotto GDB...${NC}"
    gdb --args "${BIN_PATH}" "${APP_ARGS[@]}"
elif [[ "${USE_VALGRIND}" == true ]]; then
    echo -e "${YELLOW}🔍 Avvio sotto Valgrind...${NC}"
    valgrind --leak-check=full --track-origins=yes "${BIN_PATH}" "${APP_ARGS[@]}"
else
    "${BIN_PATH}" "${APP_ARGS[@]}"
fi
