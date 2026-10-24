#!/usr/bin/env bash
# TypEmoji Uninstall Script (supports both user and sudo uninstallation)
set -euo pipefail

if [[ $EUID -eq 0 ]]; then
    PREFIX="/usr/local"
    INSTALL_BIN="$PREFIX/bin/EmojiTyper"
    INSTALL_DATA="$PREFIX/share/emojityper"
    INSTALL_APPLICATIONS="$PREFIX/share/applications/org.emojiboard.EmojiTyper.desktop"
    INSTALL_ICON="$PREFIX/share/icons/hicolor/scalable/apps/emojityper.svg"
    INSTALL_METAINFO="$PREFIX/share/metainfo/org.emojiboard.EmojiTyper.metainfo.xml"
    AUTOSTART="/etc/xdg/autostart/org.emojiboard.EmojiTyper.desktop"
    SYSTEMD_SERVICE="$PREFIX/lib/systemd/user/emojityper.service"
else
    PREFIX="${HOME}/.local"
    INSTALL_BIN="$PREFIX/bin/EmojiTyper"
    INSTALL_DATA="$PREFIX/share/emojityper"
    INSTALL_APPLICATIONS="$PREFIX/share/applications/org.emojiboard.EmojiTyper.desktop"
    INSTALL_ICON="$PREFIX/share/icons/hicolor/scalable/apps/emojityper.svg"
    INSTALL_METAINFO="$PREFIX/share/metainfo/org.emojiboard.EmojiTyper.metainfo.xml"
    AUTOSTART="${HOME}/.config/autostart/org.emojiboard.EmojiTyper.desktop"
    SYSTEMD_SERVICE="${HOME}/.config/systemd/user/emojityper.service"
fi

echo "Uninstalling TypEmoji from $PREFIX..."

# Stop running process
if pgrep -x EmojiTyper > /dev/null 2>&1; then
    pkill -x EmojiTyper 2>/dev/null || true
    echo "  ✔ Stopped running EmojiTyper process"
fi

# Stop and disable systemd service if user
if command -v systemctl &>/dev/null && [[ $EUID -ne 0 ]]; then
    systemctl --user stop emojityper.service 2>/dev/null || true
    systemctl --user disable emojityper.service 2>/dev/null || true
    echo "  ✔ Disabled systemd service"
fi

# Remove files
rm -f "$INSTALL_BIN"            && echo "  ✔ Removed binary"
rm -rf "$INSTALL_DATA"          && echo "  ✔ Removed data files"
rm -f "$INSTALL_APPLICATIONS"   && echo "  ✔ Removed desktop entry"
rm -f "$INSTALL_ICON"           && echo "  ✔ Removed icon"
rm -f "$INSTALL_METAINFO"       && echo "  ✔ Removed AppStream metadata"
rm -f "$AUTOSTART"              && echo "  ✔ Removed autostart entry"
rm -f "$SYSTEMD_SERVICE"        && echo "  ✔ Removed systemd service"

# Update desktop & icon caches
if [[ $EUID -eq 0 ]]; then
    update-desktop-database /usr/local/share/applications 2>/dev/null || true
    gtk-update-icon-cache -f -t /usr/local/share/icons/hicolor 2>/dev/null || true
else
    update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
    gtk-update-icon-cache -f -t "${HOME}/.local/share/icons/hicolor" 2>/dev/null || true
fi

echo ""
echo "  TypEmoji uninstallation complete."
