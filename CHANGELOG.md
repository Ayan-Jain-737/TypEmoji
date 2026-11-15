# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.0] - 2026-11-15

### Added
- **UI Visual Overhaul**: Spacious 396×506px floating panel with 12px drop shadow margins, 14px concentric rounded corners, and elevated depth.
- **Vector Search Bar**: Replaced raw emoji text icon with an antialiased vector magnifying glass, sleek frosted input pill, and circular hover close button.
- **Pill Category Bar**: Redesigned tab bar with smooth pill-shaped category tabs, active accent indicator, and 38px touch targets.
- **Grid Polish & Anti-Jitter**: 44×44px concentric item cells with 22pt emoji typography; eliminated text shift on hover, and added a distinct amber badge for skin tone variants.
- **Thin Overlay Scrollbars**: Minimalist 6px rounded scrollbars that integrate cleanly into dark and light themes.
- **Elevated Preview Bar**: 52px footer with a dedicated rounded preview tile, bold typography, shortcode and formatted Unicode codepoints pill tag, and keycap shortcut badges (`[↵ Insert]` `[⇧↵ Keep]`).
- **Screen Edge Clamping**: Added 12px safety margin to screen bounds so floating shadows remain completely visible.
- **Qt 6.4 Compatibility**: Added backward compatibility for Qt 6.4 (Ubuntu 24.04, Debian 12) for color scheme detection.

## [2.0.0] - 2026-11-15

### Added
- **XDG Desktop Portal Hotkey**: Global shortcut support on GNOME 44+, Sway, Hyprland, and standard Wayland desktops via `org.freedesktop.portal.GlobalShortcuts`.
- **Optional KDE Dependencies**: `KF6GlobalAccel` is now optional at build time (`-DENABLE_KDE=OFF`), allowing clean compilation and lightweight packaging on GNOME/XFCE/Debian/Fedora systems.
- **Emoji Batch Queue**: Rapid emoji clicks and keystrokes are batched into a unified clipboard paste, eliminating character drops and focus ping-pong.
- **CLI Commands**: Added `--toggle` (`-t`) and `--quit` (`-q`) command-line arguments to trigger or exit existing instances from scripts, window manager keybindings, or app launchers.
- **Flatpak Support**: Added native Flatpak packaging manifest (`org.emojiboard.EmojiTyper.flatpak.yml`).
- **X11 Modifier Independence**: `XGrabKey` now automatically handles NumLock (`Mod2Mask`) and CapsLock (`LockMask`) variations.

### Fixed
- **Dismissal Behavior**: Pressing `Escape` now immediately hides the emoji panel, regardless of search field text content.
- **App Termination**: Added `Ctrl+Q` to quit the application and `Ctrl+W` to close the panel.
- **Popup Premature Close**: Fixed issue where the panel closed unexpectedly upon opening the skin tone selector menu.
- **Target Focus Drop**: Fixed race condition where `hidePanel()` cleared target window state before the injector could restore focus.
- **Palette Theming**: Replaced hardcoded accent and hover colors with desktop environment `QPalette` roles.

### Changed
- Streamlined documentation to remove promotional AI text and focus on technical integration and desktop environment compatibility.
- Updated package specifications for Arch Linux, Debian, Fedora, openSUSE, and Snapcraft.

---

## [1.0.0] - 2026-10-24

### Added
- Initial release with Unicode 16.0 dataset (3,900+ emojis and sequences).
- System tray daemon with autostart support.
- Wayland (KWin) and X11 direct keystroke injection.
- Windows-style floating non-activating panel.
- Category navigation, search, and recent emojis tracking.
