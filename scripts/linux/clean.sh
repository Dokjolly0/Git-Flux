#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Linux Clean Utility Script
# ==============================================================================
# Utilizzo:
#   ./scripts/linux/clean.sh [OPZIONI]
#
# Opzioni:
#   -q, --qt       Elimina unicamente la build Desktop Qt (build/qt-linux)
#   -c, --core     Elimina unicamente la build Zero-Qt Core (build/core-linux)
#   -a, --all      Elimina tutte le directory di build (build/*) [default]
#   -d, --dry-run  Mostra le directory che verrebbero eliminate senza rimuoverle
#   -h, --help     Mostra questa guida
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
${BOLD}Git Flux — Script di Pulizia Build Linux${NC}

${CYAN}Utilizzo:${NC}
  $0 [OPZIONI]

${CYAN}Opzioni:${NC}
  -a, --all      Elimina l'intero contenuto della cartella build/ [default]
  -q, --qt       Elimina unicamente la build Qt Quick Desktop (build/qt-linux)
  -c, --core     Elimina unicamente la build Core Zero-Qt (build/core-linux)
  -d, --dry-run  Mostra cosa verrebbe rimosso senza cancellare nulla
  -h, --help     Mostra questa guida

${CYAN}Esempi:${NC}
  $0             # Pulisce tutte le build
  $0 --qt        # Pulisce solo build/qt-linux
  $0 --core      # Pulisce solo build/core-linux
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
            echo -e "${RED}Opzione non riconosciuta: $1${NC}" >&2
            echo "Usa $0 --help per visualizzare la guida." >&2
            exit 1
            ;;
    esac
done

cd "${ROOT_DIR}"

remove_path() {
    local target_path="$1"
    if [[ -e "${target_path}" ]]; then
        if [[ "${DRY_RUN}" == true ]]; then
            echo -e "${CYAN}[DRY-RUN] Verrebbe rimosso: ${BOLD}${target_path}${NC}"
        else
            echo -e "${YELLOW}🗑️  Rimozione: ${BOLD}${target_path}${NC}..."
            rm -rf "${target_path}"
            echo -e "${GREEN}✔ Rimosso: ${target_path}${NC}"
        fi
    else
        echo -e "${CYAN}ℹ️  Percorso non presente (già pulito): ${target_path}${NC}"
    fi
}

echo -e "${BOLD}▶ Avvio pulizia directory di build...${NC}"

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

echo -e "\n${GREEN}${BOLD}Operazione di pulizia completata!${NC}"
