#!/usr/bin/env bash
# EmojiTyper Uninstall Script
set -euo pipefail

INSTALL_BIN="${HOME}/.local/bin/EmojiTyper"
INSTALL_DATA="${HOME}/.local/share/emojityper"
INSTALL_APPLICATIONS="${HOME}/.local/share/applications/org.emojiboard.EmojiTyper.desktop"
INSTALL_ICON="${HOME}/.local/share/icons/hicolor/scalable/apps/emojityper.svg"
AUTOSTART="${HOME}/.config/autostart/org.emojiboard.EmojiTyper.desktop"
SYSTEMD_SERVICE="${HOME}/.config/systemd/user/emojityper.service"

echo "Uninstalling EmojiTyper..."

# Stop and disable service
if command -v systemctl &>/dev/null; then
    systemctl --user stop emojityper.service 2>/dev/null || true
    systemctl --user disable emojityper.service 2>/dev/null || true
    echo "  ✔ Stopped and disabled systemd service"
fi

# Kill running process
if pgrep -x EmojiTyper > /dev/null 2>&1; then
    pkill -x EmojiTyper 2>/dev/null || true
    echo "  ✔ Killed running EmojiTyper process"
fi

# Remove files
rm -f "$INSTALL_BIN"            && echo "  ✔ Removed binary"
rm -rf "$INSTALL_DATA"          && echo "  ✔ Removed data files"
rm -f "$INSTALL_APPLICATIONS"   && echo "  ✔ Removed desktop entry"
rm -f "$INSTALL_ICON"           && echo "  ✔ Removed icon"
rm -f "$AUTOSTART"              && echo "  ✔ Removed autostart entry"
rm -f "$SYSTEMD_SERVICE"        && echo "  ✔ Removed systemd service"

# Update databases
update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
gtk-update-icon-cache -f -t "${HOME}/.local/share/icons/hicolor" 2>/dev/null || true

echo ""
echo "  EmojiTyper has been uninstalled."
