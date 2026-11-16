# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.3.0] - 2026-11-16

### Fixed
- **Instant Model Viewport Repaints**: Fixed Qt model notification by emitting full-range `dataChanged` without restrictive custom roles so `QListView` unconditionally updates and repaints every cell immediately when the skin tone swatch changes.
- **Direct Corner Triangle Click**: Clicking directly on the bottom-right corner triangle indicator of any tone-capable emoji now opens the variation flyout immediately without requiring a long press or right click.
- **Flyout Outside-Dismiss Protection**: Guarded panel auto-hide during popup mouse interactions so clicking options inside the skin tone menu does not dismiss the parent panel prematurely.
- **Binary Daemon Synchronization**: Updated local launcher and user systemd service to point to the freshly built binary at `~/.local/bin/EmojiTyper`.

### Added
- **Windows-Style Common Grouper (Grid Deduplication)**: Grouped all 2,030 Unicode Fitzpatrick skin tone variants under their base emojis with 0 unmatched sequences across the entire dataset. "People & Body" and search views now present a clean, un-cluttered grid of base emojis (collapsed from 2,418 duplicate entries down to 388 base emojis).
- **Interactive Colour & Skin Tone Selector Bar**: Added a Windows 11-style horizontal swatch bar (`👋 Default (Yellow)`, `👋🏻 Light`, `👋🏼 Medium-Light`, `👋🏽 Medium`, `👋🏾 Medium-Dark`, `👋🏿 Dark`) with smart visibility (displays on tone-capable categories and search results, hides on Objects/Symbols/Flags/Kaomoji).
- **Dynamic Live Grid Tone Updates**: Switching the active tone in the selector bar instantly re-renders all tone-capable emojis in the grid to that selected skin tone via Qt model notifications without reloading or viewport flicker.
- **Per-Emoji Variation Flyout Card**: Right-clicking, long-pressing (350ms), or pressing `Space`/`Menu` on any tone-capable emoji displays an elevated Windows 11-style floating card popup with all 6 variants, highlighting the active default tone and supporting rapid one-off selection.
- **Windows 11 Subtle Corner Triangle Indicator**: Replaced circular badge with the authentic Windows 11 subtle corner triangle indicator in the bottom-right corner of variation-capable emoji cells.
- **O(1) Pre-Indexed Tone Lookups**: Added pre-indexed variant slots on `EmojiEntry` and `variantForTone()`, providing instantaneous O(1) variant lookups and preserving exact tones in Recents and frequency tracking.

## [2.2.0] - 2026-11-16

### Added
- **Configurable Global Hotkeys**: Live key sequence recorder in the Preferences dialog with native portable text serialization, modifier badges, and multi-backend synchronization across KGlobalAccel, XDG Desktop Portal, and X11 (fixes #1).
- **Recent & Frequent History JSON Export/Import**: One-click export and import for emoji usage history from Preferences with automatic deduplication, frequency merging, schema validation, and configurable capacity limits (fixes #2).
- **Curated Kaomoji Catalog Expansion**: Expanded Japanese kaomoji collection from 28 to 89 authentic expressions organized into 10 emotional categories (Greetings, Animals, Sleepy, Dancing, Bowing, Love, Shock, Shrug, Peeking, Rage/Table Flips) with complete multi-keyword search indexing (fixes #3).
- **Customizable Grid Density**: New density configuration in Appearance preferences supporting Compact (36×36px cells, 17pt font), Standard (44×44px cells, 22pt font), and Large (52×52px cells, 26pt font) with live viewport layout updates (fixes #4).

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
