#!/usr/bin/env bash
# ============================================================================
# EmojiTyper Install Script
# Builds (if needed), installs the binary, sets up autostart + systemd service
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BIN_NAME="EmojiTyper"
INSTALL_BIN="${HOME}/.local/bin"
INSTALL_DATA="${HOME}/.local/share/emojityper"
INSTALL_APPLICATIONS="${HOME}/.local/share/applications"
INSTALL_ICONS="${HOME}/.local/share/icons/hicolor"
AUTOSTART_DIR="${HOME}/.config/autostart"
SYSTEMD_DIR="${HOME}/.config/systemd/user"

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
echo "  Linux Direct Emoji Typing Tool — Installer"
echo "  ─────────────────────────────────────────"
echo ""

# ── 1. BUILD ─────────────────────────────────────────────────────────────────
step "Building EmojiTyper"

EXISTING_BIN="$BUILD_DIR/bin/$BIN_NAME"
if [[ ! -f "$EXISTING_BIN" ]]; then
    echo "  No existing build found. Running cmake + ninja..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -G Ninja \
        -DCMAKE_INSTALL_PREFIX="${HOME}/.local" \
        2>&1 | sed 's/^/  /'
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" 2>&1 | sed 's/^/  /'
    ok "Build complete"
else
    ok "Using existing build: $EXISTING_BIN (run 'cmake --build build' to rebuild)"
fi

# ── 2. INSTALL BINARY ────────────────────────────────────────────────────────
step "Installing binary to $INSTALL_BIN"

mkdir -p "$INSTALL_BIN"
cp -f "$EXISTING_BIN" "$INSTALL_BIN/$BIN_NAME"
chmod +x "$INSTALL_BIN/$BIN_NAME"
ok "Installed: $INSTALL_BIN/$BIN_NAME"

# Ensure ~/.local/bin is on PATH
if [[ ":$PATH:" != *":${HOME}/.local/bin:"* ]]; then
    warn "~/.local/bin is not in your PATH."
    echo "    Add this to your shell profile (~/.bashrc or ~/.zshrc):"
    echo "      export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

# ── 3. INSTALL DATA FILES ────────────────────────────────────────────────────
step "Installing emoji data files to $INSTALL_DATA"

mkdir -p "$INSTALL_DATA/data"
cp -f "$SCRIPT_DIR/data/emoji-test.txt"       "$INSTALL_DATA/data/"
cp -f "$SCRIPT_DIR/data/emoji-zwj-sequences.txt" "$INSTALL_DATA/data/"
ok "Data files installed"

# ── 4. INSTALL ICON ──────────────────────────────────────────────────────────
step "Installing application icon"

SVG_ICON="$SCRIPT_DIR/assets/icons/emojityper.svg"
if [[ -f "$SVG_ICON" ]]; then
    mkdir -p "$INSTALL_ICONS/scalable/apps"
    cp -f "$SVG_ICON" "$INSTALL_ICONS/scalable/apps/emojityper.svg"
    ok "Icon installed (scalable)"
    
    # Update icon cache if gtk-update-icon-cache is available
    if command -v gtk-update-icon-cache &>/dev/null; then
        gtk-update-icon-cache -f -t "$INSTALL_ICONS" 2>/dev/null && ok "Icon cache updated" || true
    fi
else
    warn "Icon not found at $SVG_ICON — icon will show a fallback"
fi

# ── 5. INSTALL DESKTOP ENTRY ─────────────────────────────────────────────────
step "Installing .desktop entry"

mkdir -p "$INSTALL_APPLICATIONS"
cat > "$INSTALL_APPLICATIONS/org.emojiboard.EmojiTyper.desktop" << EOF
[Desktop Entry]
Type=Application
Name=EmojiTyper
GenericName=Emoji Picker
Comment=Direct emoji typing tool for Linux (X11 & Wayland)
Exec=$INSTALL_BIN/$BIN_NAME
Icon=emojityper
Terminal=false
Categories=Utility;Accessibility;Qt;
Keywords=emoji;emoticon;picker;symbols;unicode;typing;
StartupNotify=false
X-KDE-GlobalAccel-Target=true
X-KDE-Shortcuts=Meta+.
Actions=ShowPanel;

[Desktop Action ShowPanel]
Name=Show Emoji Panel
Exec=$INSTALL_BIN/$BIN_NAME
EOF

ok "Desktop entry installed: $INSTALL_APPLICATIONS/org.emojiboard.EmojiTyper.desktop"

# Update desktop DB
if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$INSTALL_APPLICATIONS" 2>/dev/null && ok "Desktop database updated" || true
fi

# ── 6. SETUP XDG AUTOSTART ───────────────────────────────────────────────────
step "Setting up XDG autostart (starts EmojiTyper on login)"

mkdir -p "$AUTOSTART_DIR"
cat > "$AUTOSTART_DIR/org.emojiboard.EmojiTyper.desktop" << EOF
[Desktop Entry]
Type=Application
Name=EmojiTyper
GenericName=Emoji Picker
Comment=Direct emoji typing tool for Linux (X11 & Wayland)
Exec=$INSTALL_BIN/$BIN_NAME
Icon=emojityper
Terminal=false
Categories=Utility;Accessibility;Qt;
Keywords=emoji;emoticon;picker;symbols;unicode;typing;
StartupNotify=false
X-GNOME-Autostart-enabled=true
X-KDE-autostart-after=panel
X-KDE-GlobalAccel-Target=true
X-KDE-Shortcuts=Meta+.
EOF

ok "Autostart entry installed: $AUTOSTART_DIR/org.emojiboard.EmojiTyper.desktop"

# ── 7. SETUP SYSTEMD USER SERVICE ────────────────────────────────────────────
step "Setting up systemd user service (backup for non-autostart sessions)"

if command -v systemctl &>/dev/null; then
    mkdir -p "$SYSTEMD_DIR"
    cat > "$SYSTEMD_DIR/emojityper.service" << EOF
[Unit]
Description=EmojiTyper - Linux Direct Emoji Typer
Documentation=https://github.com/emojiboard/emojityper
After=graphical-session.target
PartOf=graphical-session.target

[Service]
Type=exec
ExecStart=$INSTALL_BIN/$BIN_NAME
Restart=on-failure
RestartSec=3
Environment=QT_QPA_PLATFORM=xcb
Environment=DISPLAY=:0

[Install]
WantedBy=graphical-session.target
EOF
    
    systemctl --user daemon-reload 2>/dev/null && ok "systemd user daemon reloaded" || warn "systemd daemon-reload failed (may not be in a session yet)"
    systemctl --user enable emojityper.service 2>/dev/null && ok "systemd service enabled" || warn "Failed to enable service (normal if not in a graphical session)"
    
    echo ""
    echo "  To start the service now:  ${BOLD}systemctl --user start emojityper${NC}"
    echo "  To check status:           ${BOLD}systemctl --user status emojityper${NC}"
    echo "  To disable autostart:      ${BOLD}systemctl --user disable emojityper${NC}"
else
    warn "systemctl not found — systemd service not installed. XDG autostart will handle startup."
fi

# ── 8. START EMOJITYPER NOW ───────────────────────────────────────────────────
step "Launching EmojiTyper in background"

# Kill any old instance first
if pgrep -x "$BIN_NAME" > /dev/null 2>&1; then
    warn "EmojiTyper is already running — sending show message..."
    "$INSTALL_BIN/$BIN_NAME" &
    sleep 1
else
    nohup "$INSTALL_BIN/$BIN_NAME" > /tmp/emojityper.log 2>&1 &
    EMOJITYPER_PID=$!
    sleep 1
    if kill -0 $EMOJITYPER_PID 2>/dev/null; then
        ok "EmojiTyper started (PID: $EMOJITYPER_PID, log: /tmp/emojityper.log)"
    else
        err "EmojiTyper failed to start. Check /tmp/emojityper.log for details."
        cat /tmp/emojityper.log || true
    fi
fi

# ── DONE ─────────────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}${BOLD}  ✔ EmojiTyper installed successfully!${NC}"
echo ""
echo "  ┌─────────────────────────────────────────────────────────┐"
echo "  │  How to use EmojiTyper                                  │"
echo "  │                                                         │"
echo "  │  • Press  Meta + .  (Win+.)  to open the emoji panel   │"
echo "  │  • Or left-click the tray icon in your system tray     │"
echo "  │  • Right-click tray icon → 'Show Emoji Panel'           │"
echo "  │                                                         │"
echo "  │  EmojiTyper will start automatically on next login.     │"
echo "  └─────────────────────────────────────────────────────────┘"
echo ""
