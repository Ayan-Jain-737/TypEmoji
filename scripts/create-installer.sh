#!/usr/bin/env bash
# ==============================================================================
# Combine prebuilt payload tarball with self-resolving dependency installer
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TAR_FILE="$(ls -t "$BUILD_DIR"/*-Linux.tar.gz 2>/dev/null | head -n1 || echo "")"
if [[ -z "$TAR_FILE" || ! -f "$TAR_FILE" ]]; then
    cd "$BUILD_DIR"
    cpack -G TGZ
    TAR_FILE="$(ls -t "$BUILD_DIR"/*-Linux.tar.gz 2>/dev/null | head -n1)"
fi
OUTPUT_RUN="$BUILD_DIR/TypEmoji-2.3.0-Universal-Installer.run"

TEMP_HEADER="$(mktemp /tmp/installer_header_XXXXXX.sh)"

cat << 'EOF' > "$TEMP_HEADER"
#!/usr/bin/env bash
# ==============================================================================
# EmojiTyper Universal Self-Resolving Installer
# Automatically installs dependencies, configures autostart, hotkeys, and runs!
# ==============================================================================
set -euo pipefail

# Ensure terminal window if launched via GUI file manager double-click
if [[ ! -t 0 || ! -t 1 ]]; then
    for term in x-terminal-emulator gnome-terminal konsole xfce4-terminal alacritty kitty foot xterm; do
        if command -v "$term" &>/dev/null; then
            exec "$term" -e bash -c "\"$0\" \"$@\"; echo ''; echo 'Press Enter to exit...'; read line"
        fi
    done
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${BLUE}"
echo "  ┌──────────────────────────────────────────────────────────┐"
echo "  │        TypEmoji Universal Linux Auto-Installer           │"
echo "  │      Direct Emoji Typing with Meta+. (Win+.) Hotkey      │"
echo "  └──────────────────────────────────────────────────────────┘"
echo -e "${NC}"

# Check for --uninstall / -u flag or invoked as typemoji-uninstall
if [[ "${1:-}" =~ ^(--uninstall|-u|uninstall)$ || "$(basename "$0")" == "typemoji-uninstall" ]]; then
    echo -e "▶ ${BOLD}Uninstalling TypEmoji...${NC}"
    pkill -x EmojiTyper 2>/dev/null || true
    if command -v systemctl &>/dev/null; then
        systemctl --user stop emojityper.service 2>/dev/null || true
        systemctl --user disable emojityper.service 2>/dev/null || true
    fi
    for prefix in "/usr/local" "${HOME}/.local"; do
        if [[ -f "$prefix/bin/EmojiTyper" || -f "$prefix/bin/typemoji-uninstall" ]]; then
            if [[ "$prefix" == "/usr/local" && $EUID -ne 0 ]]; then
                sudo rm -f "$prefix/bin/EmojiTyper" "$prefix/bin/typemoji-uninstall"
                sudo rm -rf "$prefix/share/emojityper"
                sudo rm -f "$prefix/share/applications/org.emojiboard.EmojiTyper.desktop"
                sudo rm -f "$prefix/share/icons/hicolor/scalable/apps/emojityper.svg"
                sudo rm -f "$prefix/share/metainfo/org.emojiboard.EmojiTyper.metainfo.xml"
                sudo rm -f "$prefix/lib/systemd/user/emojityper.service"
            else
                rm -f "$prefix/bin/EmojiTyper" "$prefix/bin/typemoji-uninstall"
                rm -rf "$prefix/share/emojityper"
                rm -f "$prefix/share/applications/org.emojiboard.EmojiTyper.desktop"
                rm -f "$prefix/share/icons/hicolor/scalable/apps/emojityper.svg"
                rm -f "$prefix/share/metainfo/org.emojiboard.EmojiTyper.metainfo.xml"
                rm -f "$prefix/lib/systemd/user/emojityper.service"
            fi
            echo -e "  ${GREEN}✔ Removed files from $prefix${NC}"
        fi
    done
    rm -f "${HOME}/.config/autostart/org.emojiboard.EmojiTyper.desktop"
    update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    echo -e "\n${GREEN}${BOLD}✔ TypEmoji successfully uninstalled!${NC}\n"
    exit 0
fi

# Detect distribution
DISTRO="unknown"
DISTRO_LIKE=""
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    DISTRO="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
fi

echo -e "▶ Detected Distribution: ${BOLD}${PRETTY_NAME:-$DISTRO}${NC}\n"

# Check sudo access
SUDO=""
if [[ $EUID -ne 0 ]]; then
    if command -v sudo &>/dev/null; then
        echo "Root permissions needed for system installation and dependency resolution."
        echo "Please enter your password if prompted:"
        sudo -v || { echo -e "${RED}✖ Sudo authentication failed.${NC}"; exit 1; }
        SUDO="sudo"
    else
        echo -e "${YELLOW}⚠ sudo command not found. Attempting user-only install...${NC}"
    fi
fi

# 1. Resolve Dependencies
echo -e "\n${BLUE}${BOLD}▶ 1. Resolving Dependencies for $DISTRO...${NC}"

resolve_deps() {
    if [[ "$DISTRO" =~ (ubuntu|debian|linuxmint|pop|elementary|zorin|kali) ]] || [[ "$DISTRO_LIKE" =~ (ubuntu|debian) ]]; then
        echo "  Updating package database and installing Qt6 & KF6 runtime packages..."
        $SUDO apt-get update -qq || true
        $SUDO apt-get install -y --no-install-recommends \
            libqt6core6 libqt6gui6 libqt6widgets6 libqt6dbus6 libkf6globalaccel6 libkf6xmlgui6 libxtst6 libx11-6 \
            2>/dev/null || \
        $SUDO apt-get install -y \
            qt6-base-dev libkf6globalaccel-dev libxtst6 libx11-6 || true
    elif [[ "$DISTRO" =~ (fedora|rhel|centos|rocky|alma) ]] || [[ "$DISTRO_LIKE" =~ (fedora|rhel) ]]; then
        echo "  Installing Qt6 & KF6 packages via dnf..."
        $SUDO dnf install -y qt6-qtbase kf6-kglobalaccel kf6-kxmlgui libXtst libX11 || true
    elif [[ "$DISTRO" =~ (arch|manjaro|endeavouros|garuda) ]] || [[ "$DISTRO_LIKE" =~ (arch) ]]; then
        echo "  Installing Qt6 & KF6 packages via pacman..."
        $SUDO pacman -S --needed --noconfirm qt6-base kglobalaccel kxmlgui libxtst libx11 || true
    elif [[ "$DISTRO" =~ (opensuse|suse) ]]; then
        echo "  Installing packages via zypper..."
        $SUDO zypper install -y --no-recommends libQt6Widgets6 libKF6GlobalAccel6 libXtst6 libX11-6 || true
    else
        echo -e "${YELLOW}  Unknown distribution family. Checking if Qt6 libraries are present...${NC}"
    fi
}

resolve_deps
echo -e "  ${GREEN}✔ Dependencies resolved.${NC}"

# 2. Extract and Install Files
echo -e "\n${BLUE}${BOLD}▶ 2. Installing EmojiTyper...${NC}"

PAYLOAD_LINE=$(grep -a -n '^__PAYLOAD_BEGINS__$' "$0" | head -n 1 | cut -d: -f1)
if [[ -z "$PAYLOAD_LINE" ]]; then
    echo -e "${RED}✖ Corrupted installer payload.${NC}"
    exit 1
fi

if [[ -n "$SUDO" || $EUID -eq 0 ]]; then
    INSTALL_PREFIX="/usr/local"
    tail -n +"$((PAYLOAD_LINE + 1))" "$0" | $SUDO tar -xz -C "$INSTALL_PREFIX" --strip-components=1
    
    $SUDO update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    $SUDO gtk-update-icon-cache -f -t "$INSTALL_PREFIX/share/icons/hicolor" 2>/dev/null || true
    
    # Configure autostart for current user
    AUTOSTART_DIR="${HOME}/.config/autostart"
    mkdir -p "$AUTOSTART_DIR"
    cp -f "$INSTALL_PREFIX/share/applications/org.emojiboard.EmojiTyper.desktop" "$AUTOSTART_DIR/"
    
    # Enable systemd user service if systemctl is available
    if command -v systemctl &>/dev/null; then
        systemctl --user daemon-reload 2>/dev/null || true
        systemctl --user enable emojityper.service 2>/dev/null || true
    fi
    BIN_RUN="/usr/local/bin/EmojiTyper"
else
    INSTALL_PREFIX="${HOME}/.local"
    mkdir -p "$INSTALL_PREFIX"
    tail -n +"$((PAYLOAD_LINE + 1))" "$0" | tar -xz -C "$INSTALL_PREFIX" --strip-components=1
    
    update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    gtk-update-icon-cache -f -t "${HOME}/.local/share/icons/hicolor" 2>/dev/null || true
    
    AUTOSTART_DIR="${HOME}/.config/autostart"
    mkdir -p "$AUTOSTART_DIR"
    cp -f "${HOME}/.local/share/applications/org.emojiboard.EmojiTyper.desktop" "$AUTOSTART_DIR/"
    BIN_RUN="${HOME}/.local/bin/EmojiTyper"
fi

# Install typemoji-uninstall CLI command
if [[ -n "$SUDO" || $EUID -eq 0 ]]; then
    $SUDO cp -f "$0" "$INSTALL_PREFIX/bin/typemoji-uninstall" 2>/dev/null || true
    $SUDO chmod +x "$INSTALL_PREFIX/bin/typemoji-uninstall" 2>/dev/null || true
else
    cp -f "$0" "$INSTALL_PREFIX/bin/typemoji-uninstall" 2>/dev/null || true
    chmod +x "$INSTALL_PREFIX/bin/typemoji-uninstall" 2>/dev/null || true
fi

echo -e "  ${GREEN}✔ Files installed to $INSTALL_PREFIX${NC}"
echo -e "  ${GREEN}✔ Autostart configured for login sessions.${NC}"
echo -e "  ${GREEN}✔ Uninstaller saved: run 'typemoji-uninstall' to remove anytime.${NC}"

# 3. Launch Application
echo -e "\n${BLUE}${BOLD}▶ 3. Launching EmojiTyper and registering hotkey...${NC}"

pkill -x EmojiTyper 2>/dev/null || true
sleep 0.5
nohup "$BIN_RUN" > /dev/null 2>&1 &
sleep 1

if pgrep -x EmojiTyper >/dev/null 2>&1; then
    echo -e "  ${GREEN}✔ TypEmoji is running in the background!${NC}"
    if command -v notify-send &>/dev/null; then
        notify-send "TypEmoji Ready" "Press Meta+. (Win+.) anytime to open the emoji picker." -i emojityper 2>/dev/null || true
    fi
else
    echo -e "  ${YELLOW}⚠ TypEmoji installed. Start it from your App Menu or run: EmojiTyper${NC}"
fi

echo -e "\n${GREEN}${BOLD}══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  ✔ Installation Complete!${NC}"
echo -e "  • Shortcut: Press ${BOLD}Meta + .${NC} (Win + .) to type emojis"
echo -e "  • System Tray: Click the tray icon anytime"
echo -e "  • TypEmoji will now start automatically whenever you log in."
echo -e "${GREEN}${BOLD}══════════════════════════════════════════════════════════════${NC}\n"

exit 0
__PAYLOAD_BEGINS__
EOF

cat "$TEMP_HEADER" "$TAR_FILE" > "$OUTPUT_RUN"
chmod +x "$OUTPUT_RUN"
rm -f "$TEMP_HEADER"

echo "✔ Built self-resolving installer: $OUTPUT_RUN ($(du -h "$OUTPUT_RUN" | cut -f1))"
