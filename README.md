# TypEmoji

<p align="center">
  <img src="assets/icons/emojityper.svg" alt="TypEmoji Logo" width="96" height="96">
</p>

<p align="center">
  Direct emoji picker and typing utility for Linux desktops (X11 & Wayland).<br>
  Types Unicode characters into active windows via <code>Meta + .</code> (Super + .) without overwriting user clipboard data.
</p>

<p align="center">
  <a href="https://github.com/Ayan-Jain-737/TypEmoji/releases/latest"><img src="https://img.shields.io/github/v/release/Ayan-Jain-737/TypEmoji?style=flat-square&color=blue&label=Release" alt="Latest Release"></a>
  <img src="https://img.shields.io/badge/Platform-Linux%20(X11%20%26%20Wayland)-orange?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Unicode-16.0-green?style=flat-square" alt="Unicode 16.0">
  <img src="https://img.shields.io/badge/License-MIT-purple?style=flat-square" alt="License">
</p>

---

## Overview

TypEmoji provides a non-intrusive emoji input panel for Linux. It triggers globally via `Meta + .` (or tray icon) and inputs selected emojis directly into whichever application currently has focus, restoring any previous clipboard contents immediately afterward.

### Desktop Compatibility

| Desktop Environment | Display Server | Hotkey Backend | Injection Method |
|:---------------------|:---------------|:---------------|:-----------------|
| **KDE Plasma 6**     | Wayland        | KGlobalAccel   | ydotool / KWin script |
| **KDE Plasma 5/6**   | X11            | XGrabKey       | XTest / ydotool |
| **GNOME 44+**        | Wayland        | XDG Desktop Portal / Custom shortcut | ydotool / wtype |
| **GNOME**            | X11            | XGrabKey       | XTest / xdotool |
| **Sway / Hyprland**  | Wayland        | XDG Portal / IPC (`swaymsg`) | wtype / ydotool |
| **XFCE / MATE / i3** | X11            | XGrabKey       | XTest / xdotool |

---

## Installation

Precompiled packages for major distributions are available on the **[Releases Page](https://github.com/Ayan-Jain-737/TypEmoji/releases/latest)**.

### Universal Installer (`.run`)
Works across Ubuntu, Debian, Fedora, Arch Linux, openSUSE, and derivatives:
```bash
chmod +x TypEmoji-2.0.0-Universal-Installer.run
./TypEmoji-2.0.0-Universal-Installer.run
```

### Ubuntu / Debian / Linux Mint (`.deb`)
```bash
sudo apt install ./TypEmoji-2.0.0-Ubuntu-Debian-x86_64.deb
```

### Fedora / RHEL / openSUSE (`.rpm`)
```bash
sudo dnf install ./TypEmoji-2.0.0-Fedora-openSUSE-x86_64.rpm
# On openSUSE:
# sudo zypper install ./TypEmoji-2.0.0-Fedora-openSUSE-x86_64.rpm
```

### Arch Linux / Manjaro (`.pkg.tar.zst`)
```bash
sudo pacman -U ./TypEmoji-2.0.0-Arch-Linux-x86_64.pkg.tar.zst
```

---

## Usage

- **Open Picker**: Press `Meta + .` (Win + .) or click the system tray icon.
- **Search**: Type immediately when the picker opens to filter emojis by name or keyword.
- **Select & Type**:
  - `Click` or `Enter`: Types emoji and closes the panel.
  - `Shift + Click` or `Shift + Enter`: Types emoji and keeps the panel open for multi-emoji typing.
- **Dismiss**: Press `Escape` or `Ctrl + W` (or click outside the window).
- **Exit Daemon**: Press `Ctrl + Q` inside the panel, right-click tray -> **Exit**, or run `EmojiTyper --quit`.

### Command-Line Interface

```bash
EmojiTyper            # Launch background daemon (or open panel if already running)
EmojiTyper --toggle   # Toggle panel visibility (ideal for custom WM keybindings)
EmojiTyper --show     # Force show panel
EmojiTyper --quit     # Gracefully terminate background daemon
EmojiTyper --reload   # Seamlessly reload updated binary
```

---

## Building from Source

### Dependencies

- **C++23 compiler** (GCC 13+ or Clang 16+)
- **CMake 3.25+** and **Ninja**
- **Qt 6** (`Core`, `Gui`, `Widgets`, `Network`, `Concurrent`, `DBus`)
- **X11 libraries** (`libX11`, `libXtst`)
- **Optional**: `KF6GlobalAccel` and `KF6XmlGui` (for native KDE Wayland shortcuts)

#### Arch Linux:
```bash
sudo pacman -S --needed cmake ninja gcc qt6-base qt6-tools libxtst libx11 noto-fonts-emoji
# Optional KDE support:
sudo pacman -S --needed kglobalaccel kxmlgui
```

#### Ubuntu / Debian:
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-tools-dev libx11-dev libxtst-dev fonts-noto-color-emoji
# Optional KDE support:
sudo apt install libkf6globalaccel-dev libkf6xmlgui-dev
```

#### Fedora:
```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qttools-devel libX11-devel libXtst-devel google-noto-color-emoji-fonts
# Optional KDE support:
sudo dnf install kf6-kglobalaccel-devel kf6-kxmlgui-devel
```

### Build & Install

```bash
git clone https://github.com/Ayan-Jain-737/TypEmoji.git
cd TypEmoji

# Standard build (auto-detects KDE support)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Or explicitly disable KDE dependencies on pure GNOME/XFCE systems:
# cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_KDE=OFF

cmake --build build --parallel

# Install system-wide
sudo cmake --install build
```

---

## Architecture

```
+-------------------------------------------------------------+
|                     Desktop Session                         |
|      (KDE Plasma, GNOME, Sway, Hyprland, XFCE, i3)          |
+-------------------------------------------------------------+
                               |
                    Global Shortcut / Tray
                               |
                               v
+-------------------------------------------------------------+
|                        TrayHost                             |
|    - SingleInstance local socket IPC (--toggle, --quit)     |
|    - System tray icon & XDG autostart management            |
+-------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------+
|                       EmojiPanel                            |
|    - Floating frameless panel                               |
|    - Dynamic QPalette adaptive theming (Light/Dark)         |
|    - Fast search indexing & Unicode 16.0 catalog            |
|    - Skin tone variant selector & Kaomoji collection        |
+-------------------------------------------------------------+
                               | (Selection)
                               v
+-------------------------------------------------------------+
|                     EmojiInjector                           |
|    - Batched emoji queue (prevents dropped rapid typing)    |
|    - Target window focus verification                       |
|    - Cascading injection: ydotool -> wtype -> XTest/xdotool |
|    - User clipboard backup and automatic restoration        |
+-------------------------------------------------------------+
```

---

## License

TypEmoji is licensed under the [MIT License](LICENSE).  
Emoji data sourced in accordance with the [Unicode Terms of Use](https://www.unicode.org/copyright.html).
