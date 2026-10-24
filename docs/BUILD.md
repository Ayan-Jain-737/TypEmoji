# TypEmoji Build Guide (Linux)

## Prerequisites

- Linux (tested on Arch Linux, Ubuntu, Fedora)
- CMake 3.25+
- Qt 6.5+ (Core, Gui, Widgets, Network, Concurrent, DBus)
- GCC 12+ or Clang 15+ with C++23 support
- X11 development headers (`libx11-dev`, `libxtst-dev`)
- KDE Frameworks 6 (`KF6GlobalAccel`, `KF6XmlGui`)
- wtype (optional, for Wayland native injection)
- Noto Color Emoji font

## Quick Install Dependencies

**Arch Linux / Manjaro:**
```bash
sudo pacman -S --needed cmake ninja gcc qt6-base qt6-tools kglobalaccel kxmlgui libxtst libx11 noto-fonts-emoji
```

**Ubuntu / Debian:**
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-tools-dev libkf6globalaccel-dev libkf6xmlgui-dev libx11-dev libxtst-dev fonts-noto-color-emoji
```

**Fedora:**
```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qttools-devel kf6-kglobalaccel-devel kf6-kxmlgui-devel libX11-devel libXtst-devel google-noto-color-emoji-fonts
```

## Quick Build

```bash
git clone https://github.com/Ayan-Jain-737/TypEmoji.git
cd TypEmoji
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel
```

## Running Locally

```bash
./build/bin/EmojiTyper
```

The app runs in the system tray. Press **Super + .** (Win + .) to open the emoji panel.

## Native Hotkey Replacement

The native Super+. emoji panel is replaced by native interception:
1. App starts
2. Captures Super+Period on root window via KGlobalAccel / XGrabKey
3. Event is handled directly
4. Native panel never opens
5. TypEmoji panel opens instead

No system config changes needed for basic functionality.

## Packaging

### CPack TGZ / DEB / RPM

```bash
cd build
cpack -G "DEB;TGZ"
```

### Universal Self-Resolving Installer

```bash
./scripts/create-installer.sh
```
Outputs `build/EmojiTyper-1.0.0-Installer.run` which runs on any Linux distribution.