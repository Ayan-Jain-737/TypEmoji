#!/usr/bin/env bash
# ============================================================================
# EmojiTyper Install Script
# Supports both user-level installation and system-wide sudo installation
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_NAME="EmojiTyper"

# Detect root / sudo vs standard user installation
if [[ $EUID -eq 0 ]]; then
    IS_ROOT=true
    INSTALL_PREFIX="/usr/local"
    INSTALL_BIN="$INSTALL_PREFIX/bin"
    INSTALL_DATA="$INSTALL_PREFIX/share/emojityper"
    INSTALL_APPLICATIONS="$INSTALL_PREFIX/share/applications"
    INSTALL_ICONS="$INSTALL_PREFIX/share/icons/hicolor"
    INSTALL_METAINFO="$INSTALL_PREFIX/share/metainfo"
    SYSTEMD_DIR="$INSTALL_PREFIX/lib/systemd/user"
    AUTOSTART_DIR="/etc/xdg/autostart"
else
    IS_ROOT=false
    INSTALL_PREFIX="${HOME}/.local"
    INSTALL_BIN="$INSTALL_PREFIX/bin"
    INSTALL_DATA="$INSTALL_PREFIX/share/emojityper"
    INSTALL_APPLICATIONS="$INSTALL_PREFIX/share/applications"
    INSTALL_ICONS="$INSTALL_PREFIX/share/icons/hicolor"
    INSTALL_METAINFO="$INSTALL_PREFIX/share/metainfo"
    SYSTEMD_DIR="${HOME}/.config/systemd/user"
    AUTOSTART_DIR="${HOME}/.config/autostart"
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

step() { echo -e "\n${BLUE}${BOLD}▶ $1${NC}"; }
ok()   { echo -e "  ${GREEN}✔ $1${NC}"; }
warn() { echo -e "  ${YELLOW}⚠ $1${NC}"; }
err()  { echo -e "  ${RED}✖ $1${NC}"; }

echo -e "${BOLD}"
echo "  ███████╗███╗   ███╗ ██████╗      ██╗██╗████████╗██╗   ██╗██████╗ ███████╗██████╗ "
echo "  ██╔════╝████╗ ████║██╔═══██╗     ██║██║╚══██╔══╝╚██╗ ██╔╝██╔══██╗██╔════╝██╔══██╗"
echo "  █████╗  ██╔████╔██║██║   ██║     ██║██║   ██║    ╚████╔╝ ██████╔╝█████╗  ██████╔╝"
echo "  ██╔══╝  ██║╚██╔╝██║██║   ██║██   ██║██║   ██║     ╚██╔╝  ██╔═══╝ ██╔══╝  ██╔══██╗"
echo "  ███████╗██║ ╚═╝ ██║╚██████╔╝╚█████╔╝██║   ██║      ██║   ██║     ███████╗██║  ██║"
echo "  ╚══════╝╚═╝     ╚═╝ ╚═════╝  ╚════╝ ╚═╝   ╚═╝      ╚═╝   ╚═╝     ╚══════╝╚═╝  ╚═╝"
echo -e "${NC}"
if [[ "$IS_ROOT" == true ]]; then
    echo "  Linux Direct Emoji Typing Tool — System-Wide Installer (sudo)"
else
    echo "  Linux Direct Emoji Typing Tool — User Installer"
fi
echo "  ─────────────────────────────────────────────────────────────"
echo ""

# ── 1. BUILD ─────────────────────────────────────────────────────────────────
step "Building EmojiTyper"

mkdir -p "$BUILD_DIR"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -G Ninja \
    2>&1 | sed 's/^/  /'
cmake --build "$BUILD_DIR" --parallel "$(nproc)" 2>&1 | sed 's/^/  /'
ok "Build complete"

# ── 2. INSTALL VIA CMAKE ─────────────────────────────────────────────────────
step "Installing files to $INSTALL_PREFIX"

DESTDIR="" cmake --install "$BUILD_DIR" 2>&1 | sed 's/^/  /'
ok "Installed binary, desktop entry, icon, data files, and AppStream metadata"

# Ensure ~/.local/bin is in PATH for user installs
if [[ "$IS_ROOT" == false && ":$PATH:" != *":${HOME}/.local/bin:"* ]]; then
    warn "~/.local/bin is not in your PATH."
    echo "    Add this to your shell profile (~/.bashrc or ~/.zshrc):"
    echo "      export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

# Update caches
if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$INSTALL_APPLICATIONS" 2>/dev/null && ok "Desktop database updated" || true
fi
if command -v gtk-update-icon-cache &>/dev/null; then
    gtk-update-icon-cache -f -t "$INSTALL_ICONS" 2>/dev/null && ok "Icon cache updated" || true
fi

# ── 3. SETUP AUTOSTART / SYSTEMD ─────────────────────────────────────────────
if [[ "$IS_ROOT" == true ]]; then
    mkdir -p "$AUTOSTART_DIR"
    cp -f "$PROJECT_ROOT/org.emojiboard.EmojiTyper.desktop" "$AUTOSTART_DIR/"
    ok "System autostart configured in $AUTOSTART_DIR"
else
    mkdir -p "$AUTOSTART_DIR"
    cp -f "$PROJECT_ROOT/org.emojiboard.EmojiTyper.desktop" "$AUTOSTART_DIR/"
    ok "User autostart configured in $AUTOSTART_DIR"

    if command -v systemctl &>/dev/null; then
        systemctl --user daemon-reload 2>/dev/null || true
        systemctl --user enable emojityper.service 2>/dev/null && ok "systemd service enabled" || true
    fi

    # Launch daemon for current session if user install
    step "Launching EmojiTyper in background"
    if pgrep -x "$BIN_NAME" > /dev/null 2>&1; then
        warn "EmojiTyper is already running — sending show message..."
        "$INSTALL_BIN/$BIN_NAME" &
    else
        nohup "$INSTALL_BIN/$BIN_NAME" > /tmp/emojityper.log 2>&1 &
        sleep 1
        if kill -0 $! 2>/dev/null; then
            ok "EmojiTyper started (PID: $!, log: /tmp/emojityper.log)"
        fi
    fi
fi

# ── DONE ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}  ✔ TypEmoji installed successfully!${NC}"
echo ""
echo "  ┌─────────────────────────────────────────────────────────┐"
echo "  │  How to use TypEmoji                                    │"
echo "  │                                                         │"
echo "  │  • Press  Meta + .  (Win+.)  to open the emoji panel   │"
echo "  │  • Or click the tray icon in your system tray           │"
echo "  │                                                         │"
echo "  │  TypEmoji will start automatically on next login.       │"
echo "  └─────────────────────────────────────────────────────────┘"
echo ""
