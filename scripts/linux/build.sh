#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Build Utility Script
# ==============================================================================
# Utilizzo:
#   ./scripts/linux/build.sh [OPZIONI]
#
# Opzioni:
#   -q, --qt            Compila il preset Desktop Qt Quick 6 (default: linux-qt-release)
#   -c, --core          Compila il preset Zero-Qt Core C++23 (linux-core-release)
#   -a, --all           Compila entrambi i preset (core + desktop)
#   -C, --clean         Pulisce la directory di build prima di compilare
#   -g, --configure     Forza la fase di configurazione cmake prima della compilazione
#   -t, --target NOME   Compila solo lo specifico target (es. git_flux, gf_unit_tests)
#   -j, --jobs NUM      Numero di job paralleli (default: nproc)
#   -v, --verbose       Output dettagliato durante la compilazione
#   -h, --help          Mostra questo messaggio di aiuto
# ==============================================================================

set -euo pipefail

# Colori per il terminale
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Directory radice del repository
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Parametri di default
PRESET="linux-qt-release"
BUILD_ALL=false
CLEAN_BUILD=false
FORCE_CONFIGURE=false
SPECIFIC_TARGET=""
JOBS=$(nproc 2>/dev/null || echo 4)
VERBOSE=false

show_help() {
    cat << EOF
${BOLD}Git Flux — Script di Compilazione Linux${NC}

${CYAN}Utilizzo:${NC}
  $0 [OPZIONI]

${CYAN}Opzioni:${NC}
  -q, --qt             Compila il preset Desktop Qt Quick 6 [default] (${BOLD}linux-qt-release${NC})
  -c, --core           Compila il preset Zero-Qt Core C++23 (${BOLD}linux-core-release${NC})
  -a, --all            Compila sia Core che Desktop in sequenza
  -C, --clean          Elimina la directory di build prima di compilare
  -g, --configure      Forza la riconfigurazione di CMake prima della build
  -t, --target <NOME>  Compila unicamente il target specificato (es. git_flux)
  -j, --jobs <NUM>     Numero di job paralleli di compilazione (default: ${JOBS})
  -v, --verbose        Modalità verbosa (stampa linee di comando del compilatore)
  -h, --help           Mostra questa guida

${CYAN}Esempi:${NC}
  $0                         # Build completa del target desktop standard
  $0 --core                  # Build del core puro C++23 (Zero-Qt)
  $0 --all                   # Build completa di entrambi i preset
  $0 --clean --qt            # Clean build del desktop
  $0 -t git_flux             # Compila solo l'eseguibile principale
  $0 -t gf_viewmodels_tests  # Compila solo l'eseguibile dei test MVVM
EOF
}

# Parsing argomenti
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
                echo -e "${RED}Errore: --target richiede il nome di un target.${NC}" >&2
                exit 1
            fi
            SPECIFIC_TARGET="$2"
            shift 2
            ;;
        -j|--jobs)
            if [[ -z "${2:-}" ]]; then
                echo -e "${RED}Errore: --jobs richiede un numero.${NC}" >&2
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
            echo -e "${RED}Opzione non riconosciuta: $1${NC}" >&2
            echo "Usa $0 --help per visualizzare la guida." >&2
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
    echo -e "${BOLD}${CYAN}▶ Avvio compilazione per preset: ${YELLOW}${target_preset}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ "${CLEAN_BUILD}" == true && -n "${build_dir}" && -d "${build_dir}" ]]; then
        echo -e "${YELLOW}🧹 Pulizia directory di build: ${build_dir}...${NC}"
        rm -rf "${build_dir}"
    fi

    if [[ "${FORCE_CONFIGURE}" == true || ! -d "${build_dir}" ]]; then
        echo -e "${CYAN}⚙️  Configurazione CMake con preset '${target_preset}'...${NC}"
        cmake --preset "${target_preset}"
    fi

    local build_cmd=("cmake" "--build" "--preset" "${target_preset}" "-j" "${JOBS}")

    if [[ -n "${SPECIFIC_TARGET}" ]]; then
        echo -e "${CYAN}🎯 Compilazione target specifico: ${BOLD}${SPECIFIC_TARGET}${NC}"
        build_cmd+=("--target" "${SPECIFIC_TARGET}")
    fi

    if [[ "${VERBOSE}" == true ]]; then
        build_cmd+=("--verbose")
    fi

    local start_time
    start_time=$(date +%s)

    echo -e "${CYAN}🔨 Esecuzione: ${build_cmd[*]}${NC}"
    "${build_cmd[@]}"

    local end_time
    end_time=$(date +%s)
    local elapsed=$((end_time - start_time))

    echo -e "${GREEN}✔ Compilazione '${target_preset}' completata con successo in ${elapsed}s!${NC}\n"
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
echo -e "${GREEN}${BOLD}  TUTTE LE OPERAZIONI DI BUILD SONO STATE COMPLETATE (${TOTAL_ELAPSED}s)${NC}"
echo -e "${GREEN}${BOLD}============================================================${NC}"
