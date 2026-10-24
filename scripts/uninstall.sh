#!/usr/bin/env bash
# ==============================================================================
# TypEmoji Comprehensive Uninstaller
# Cleans up processes, autostart, systemd services, desktop shortcuts, and files
# ==============================================================================
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${BLUE}"
echo "  ┌──────────────────────────────────────────────────────────┐"
echo "  │             TypEmoji Uninstallation Wizard               │"
echo "  └──────────────────────────────────────────────────────────┘"
echo -e "${NC}"

# 1. Stop running processes
echo "▶ 1. Stopping running TypEmoji processes..."
if pgrep -x EmojiTyper >/dev/null 2>&1; then
    pkill -x EmojiTyper 2>/dev/null || true
    echo -e "  ${GREEN}✔ Terminated running daemon.${NC}"
fi

# 2. Stop and disable systemd user services
if command -v systemctl &>/dev/null; then
    systemctl --user stop emojityper.service 2>/dev/null || true
    systemctl --user disable emojityper.service 2>/dev/null || true
    if [[ $EUID -eq 0 ]]; then
        systemctl --global disable emojityper.service 2>/dev/null || true
    fi
fi

# 3. Clean files from /usr/local and ~/.local
echo "▶ 2. Removing application files..."
PREFIXES=("/usr/local" "${HOME}/.local")

for p in "${PREFIXES[@]}"; do
    if [[ -d "$p" ]]; then
        if [[ "$p" == "/usr/local" && $EUID -ne 0 ]]; then
            SUDO="sudo"
        else
            SUDO=""
        fi

        if [[ -f "$p/bin/EmojiTyper" || -f "$p/bin/typemoji-uninstall" ]]; then
            $SUDO rm -f "$p/bin/EmojiTyper" "$p/bin/typemoji-uninstall"
            $SUDO rm -rf "$p/share/emojityper"
            $SUDO rm -f "$p/share/applications/org.emojiboard.EmojiTyper.desktop"
            $SUDO rm -f "$p/share/icons/hicolor/scalable/apps/emojityper.svg"
            $SUDO rm -f "$p/share/metainfo/org.emojiboard.EmojiTyper.metainfo.xml"
            $SUDO rm -f "$p/lib/systemd/user/emojityper.service"
            echo -e "  ${GREEN}✔ Removed files from $p${NC}"
        fi
    fi
done

# 4. Remove Autostart Entries
echo "▶ 3. Removing autostart entries..."
rm -f "${HOME}/.config/autostart/org.emojiboard.EmojiTyper.desktop"
if [[ $EUID -eq 0 ]] || command -v sudo &>/dev/null; then
    sudo rm -f "/etc/xdg/autostart/org.emojiboard.EmojiTyper.desktop" 2>/dev/null || true
fi
rm -f "${HOME}/.config/systemd/user/emojityper.service"

# 5. Update system caches
echo "▶ 4. Refreshing desktop and icon databases..."
update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
if command -v sudo &>/dev/null && [[ $EUID -eq 0 ]]; then
    update-desktop-database /usr/local/share/applications 2>/dev/null || true
    gtk-update-icon-cache -f -t /usr/local/share/icons/hicolor 2>/dev/null || true
fi

echo -e "\n${GREEN}${BOLD}✔ TypEmoji has been completely uninstalled from your system.${NC}\n"

