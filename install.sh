#!/usr/bin/env bash
# ==============================================================================
# Git Flux — Universal Linux One-Liner Installer
# ==============================================================================
set -euo pipefail

BOLD="\033[1m"
GREEN="\033[32m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

echo -e "${CYAN}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"
echo -e "${GREEN}${BOLD}🚀 Git Flux — Installing Git Flux on Linux${RESET}"
echo -e "${CYAN}${BOLD}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RESET}"

if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}✖ Please run this installer with root privileges (e.g. sudo bash install.sh)${RESET}"
    exit 1
fi

BASE_URL="https://dokjolly0.github.io/Git-Flux"

# Detect Distribution
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO_ID="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
else
    DISTRO_ID="unknown"
    DISTRO_LIKE=""
fi

case "${DISTRO_ID}" in
    ubuntu|debian|linuxmint|pop|elementary|zorin)
        echo -e "${YELLOW}▶ Detected Debian/Ubuntu family (${DISTRO_ID}). Setting up APT repository...${RESET}"
        mkdir -p /etc/apt/keyrings
        curl -fsSL "${BASE_URL}/KEY.gpg" | gpg --dearmor --yes -o /etc/apt/keyrings/gitflux.gpg
        echo "deb [signed-by=/etc/apt/keyrings/gitflux.gpg] ${BASE_URL}/ ./" > /etc/apt/sources.list.d/gitflux.list
        apt-get update -qq
        apt-get install -y git-flux
        echo -e "${GREEN}${BOLD}✔ Git Flux successfully installed via APT!${RESET}"
        ;;

    fedora|rhel|centos|rocky|almalinux)
        echo -e "${YELLOW}▶ Detected Fedora/RHEL family (${DISTRO_ID}). Installing RPM...${RESET}"
        dnf install -y "https://github.com/Dokjolly0/Git-Flux/releases/download/v0.1.0/git-flux-0.1.0-1.x86_64.rpm" || \
        rpm -Uvh "https://github.com/Dokjolly0/Git-Flux/releases/download/v0.1.0/git-flux-0.1.0-1.x86_64.rpm"
        echo -e "${GREEN}${BOLD}✔ Git Flux successfully installed via RPM!${RESET}"
        ;;

    arch|manjaro|endeavouros)
        echo -e "${YELLOW}▶ Detected Arch Linux family (${DISTRO_ID}). Installing Arch package...${RESET}"
        pacman -U --noconfirm "https://github.com/Dokjolly0/Git-Flux/releases/download/v0.1.0/git-flux-0.1.0-1-x86_64.pkg.tar.zst"
        echo -e "${GREEN}${BOLD}✔ Git Flux successfully installed via Pacman!${RESET}"
        ;;

    *)
        if [[ "${DISTRO_LIKE}" =~ (debian|ubuntu) ]]; then
            echo -e "${YELLOW}▶ Detected Debian-like system. Setting up APT repository...${RESET}"
            mkdir -p /etc/apt/keyrings
            curl -fsSL "${BASE_URL}/KEY.gpg" | gpg --dearmor --yes -o /etc/apt/keyrings/gitflux.gpg
            echo "deb [signed-by=/etc/apt/keyrings/gitflux.gpg] ${BASE_URL}/ ./" > /etc/apt/sources.list.d/gitflux.list
            apt-get update -qq
            apt-get install -y git-flux
            echo -e "${GREEN}${BOLD}✔ Git Flux successfully installed!${RESET}"
        else
            echo -e "${RED}✖ Unsupported distribution: ${DISTRO_ID}. Please download the portable tarball from:${RESET}"
            echo -e "  https://github.com/Dokjolly0/Git-Flux/releases/latest"
            exit 1
        fi
        ;;
esac

echo -e "${GREEN}${BOLD}🎉 You can now launch Git Flux by running 'git_flux' or from your application menu!${RESET}"
