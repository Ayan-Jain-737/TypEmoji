## TypEmoji v2.3.0 — Windows-Style Common Grouper, Live Skin Tone Selector & Hotkey Customization

TypEmoji v2.3.0 brings the native Windows 11 emoji picker experience to Linux desktops with smart skin tone deduplication, interactive colour selection, configurable shortcuts, history management, and custom display densities.

---

### What's New in v2.3.0

#### 1. Windows-Style Common Grouper (Grid Deduplication)
- Grouped all **2,030 Unicode Fitzpatrick skin tone variants** cleanly under their base emojis with 0 unmatched sequences across the entire Unicode dataset.
- Emojis in "People & Body" and search views are collapsed from 2,418 duplicate entries down to **388 clean base emojis**, eliminating clutter and visual fatigue.

#### 2. Interactive Colour & Skin Tone Selector Bar
- Windows 11-style horizontal swatch bar (`👋 Default (Yellow)`, `👋🏻 Light`, `👋🏼 Medium-Light`, `👋🏽 Medium`, `👋🏾 Medium-Dark`, `👋🏿 Dark`).
- Switching the active tone in the selector bar instantly re-renders all tone-capable emojis in the grid live without reload or viewport flicker.
- Smart auto-visibility: displayed on tone-capable categories ("People & Body") and search results, seamlessly hidden on Objects, Symbols, Flags, and Kaomoji.

#### 3. Direct Corner Triangle Click & Variation Flyout Card
- Replaced old static badges with the authentic Windows 11 subtle corner triangle indicator in the bottom-right corner of variation-capable emoji cells.
- Direct click on the corner triangle, right-click, long-press (350ms), or `Space`/`Menu` key immediately pops up an elevated 6-variant floating card flyout.
- High-contrast active ring highlights your current default tone while allowing effortless one-off variant injection.

#### 4. Configurable Global Hotkeys with Live Key Recorder
- Interactive hotkey recorder inside Preferences: record any custom shortcut sequence with native modifier badges (`Super`, `Ctrl`, `Alt`, `Shift`).
- Cross-backend synchronization across **KGlobalAccel** (KDE Wayland), **XDG Desktop Portal** (GNOME Wayland, Sway, Hyprland), and **X11 / XGrabKey**.

#### 5. Emoji Usage History JSON Export & Import
- Complete backup and restore for recent and frequent emoji history in clean, standardized JSON format.
- Automatic deduplication, frequency counter merging, and configurable capacity limits.

#### 6. Curated Japanese Kaomoji Catalog Expansion
- Expanded from 28 to **89 authentic Japanese kaomoji expressions** across 10 emotional categories (Greetings, Animals, Sleepy, Dancing, Bowing, Love, Shock, Shrug, Peeking, Table Flips/Rage).
- Full multi-keyword search indexing (e.g., search "cat", "happy", "shrug", "flip", "hug").

#### 7. Customizable Grid Density
- Choose your preferred cell layout in Appearance preferences:
  - **Compact**: 36×36px cells, 17pt font
  - **Standard**: 44×44px cells, 22pt font
  - **Large**: 52×52px cells, 26pt font
- Live viewport geometry updates without requiring an application restart.

---

### Bug Fixes & Engine Hardening

- **Instant Model Viewport Repaints**: Fixed Qt model notification by emitting full-range `dataChanged` without restrictive custom roles so `QListView` unconditionally updates and repaints every cell immediately when the skin tone swatch changes.
- **Flyout Outside-Dismiss Protection**: Guarded panel auto-hide during popup mouse interactions so clicking options inside the skin tone menu does not dismiss the parent panel prematurely.
- **Binary Daemon Synchronization**: Updated local launcher and user systemd service to point to the freshly built binary at `~/.local/bin/EmojiTyper`.

---

### Installation & Downloads

#### One-Command Quick Install
```bash
curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash
```

#### Precompiled Native Packages
| Distribution | Install Command |
| :--- | :--- |
| **Arch Linux / Manjaro** | `sudo pacman -U https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Arch-Linux-x86_64.pkg.tar.zst` |
| **Ubuntu / Debian / Mint** | `sudo apt install ./TypEmoji-2.3.0-Ubuntu-Debian-x86_64.deb` |
| **Fedora / openSUSE / RHEL** | `sudo dnf install https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.0/TypEmoji-2.3.0-Fedora-openSUSE-x86_64.rpm` |
| **Universal Linux (.run)** | `chmod +x TypEmoji-2.3.0-Universal-Installer.run && ./TypEmoji-2.3.0-Universal-Installer.run` |
| **Portable Tarball** | `tar -xzf TypEmoji-2.3.0-Portable-Linux-x86_64.tar.gz && ./TypEmoji/bin/EmojiTyper` |

---

🎮 **Try the live web demo**: [https://ayan-jain-737.github.io/TypEmoji/](https://ayan-jain-737.github.io/TypEmoji/)
