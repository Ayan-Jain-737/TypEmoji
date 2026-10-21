# EmojiTyper Build Guide (Linux)

## Prerequisites

- Linux (tested on Arch Linux)
- CMake 3.25+
- Qt 6.5+ (Core, Gui, Widgets, Network, Concurrent)
- GCC 12+ or Clang 15+ with C++23 support
- X11 development headers
- XTest library
- wtype (optional, for Wayland native injection)
- Noto Color Emoji font

## Quick Install (Arch Linux)

```bash
sudo pacman -S --needed \
  cmake qt6-base qt6-tools gcc \
  libxtst wtype noto-fonts-emoji
```

## Quick Build

```bash
cd /home/furious/Projects/emojiboard
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build . --parallel
```

## Running

```bash
./build/bin/EmojiTyper
```

The app runs in system tray. Press **Super + .** to open emoji panel.

## Run Tests

```bash
cd build
ctest --output-on-failure
```

## Native Hotkey Replacement

The native Super+. emoji panel is replaced by XGrabKey interception:
1. App starts
2. XGrabKey captures Super+Period on root window
3. Event is swallowed (not passed to compositor/WM)
4. Native panel never opens
5. Our panel opens instead

No system config changes needed for basic functionality.

Optional: Disable native pickers via Settings UI (GNOME/KDE specific)

## Project Stats

- Files: 24 source files
- Lines of code: ~5,000+
- Structure: Modern C++23 + Qt6
- Features: Direct typing, non-activating panel, single instance, settings persistence
- Platform: Linux X11/Wayland (Xwayland)

## Packaging

### CPack TGZ (Portable)

```bash
cd build
cmake --install . --prefix ../dist
cpack -G TGZ
```

### AppImage (Future)

```bash
# Using linuxdeployqt
linuxdeployqt ./bin/EmojiTyper -appimage
```