## TypEmoji v2.0.0 — Universal Linux Emoji Typing

TypEmoji v2.0.0 brings comprehensive multi-distribution support across all major Linux desktop environments (KDE Plasma, GNOME, Sway, Hyprland, XFCE, i3), zero-drop batched emoji typing, and native system integration.

### What's New
- **XDG Desktop Portal Hotkey**: Global shortcut support on GNOME 44+, Sway, Hyprland, and standard Wayland desktops via `org.freedesktop.portal.GlobalShortcuts`.
- **Optional KDE Dependencies**: `KF6GlobalAccel` is now optional at build time (`-DENABLE_KDE=OFF`), allowing clean compilation and lightweight packaging on GNOME, XFCE, Debian, and Fedora.
- **Emoji Batch Queue**: Rapid emoji clicks and keystrokes are batched into a unified clipboard paste, eliminating character drops and focus ping-pong.
- **CLI Commands**: Added `--toggle` (`-t`) and `--quit` (`-q`) command-line arguments to trigger or exit existing instances from scripts, window manager keybindings, or app launchers.
- **Flatpak Support**: Added native Flatpak packaging manifest (`packaging/org.emojiboard.EmojiTyper.flatpak.yml`).
- **X11 Modifier Independence**: `XGrabKey` now automatically handles NumLock (`Mod2Mask`) and CapsLock (`LockMask`) variations.
- **Modernized UI**: Adaptive system `QPalette` highlight colors, soft floating drop shadow, formatted Unicode codepoints preview, and stable popup menus.

### Installation Packages
Once tags are pushed, GitHub Actions automatically builds and attaches the following release assets:
- `TypEmoji-2.0.0-Universal-Installer.run` (All Linux distros)
- `TypEmoji-2.0.0-Ubuntu-Debian-x86_64.deb` (Debian, Ubuntu, Mint)
- `TypEmoji-2.0.0-Fedora-openSUSE-x86_64.rpm` (Fedora, openSUSE, RHEL)
- `TypEmoji-2.0.0-Arch-Linux-x86_64.pkg.tar.zst` (Arch Linux, Manjaro)
- `TypEmoji-2.0.0-Portable-Linux-x86_64.tar.gz` (Standalone tarball)
