# EmojiTyper — Direct Emoji Typing Tool for Linux (X11 & Wayland)

<p align="center">
  <img src="assets/icons/emojityper.svg" alt="EmojiTyper Logo" width="96" height="96">
</p>

<p align="center">
  <b>A modern, responsive, non-activating emoji picker and direct typing tool for Linux.</b><br>
  Built with C++23, Qt6, and native Linux desktop integrations.
</p>

---

## Highlights

- 🚀 **Direct Injection**: Types selected emojis directly into focused text fields via native Wayland (`wtype` / `wl-clipboard`) and X11 (`XTest` / `xclip`) pipelines.
- ⚡ **Non-Activating Window**: Stays on top without stealing focus from your active text editor or browser.
- ⌨️ **Global Hotkey (`Super + .`)**: Native shortcut integration via KDE KGlobalAccel and X11 XGrabKey.
- 🎨 **Modern UI/UX**: Designed for visual parity with Plasma EmojiSelector and Windows Emoji Panel with dark/light theme support.
- 🔍 **Instant Search & Standard Categories**: Organizes 3,900+ Unicode emojis in standard Unicode order with instant prefix and keyword search.
- ⏱️ **Recent & Frequent Emojis**: Tracks your most used emojis across sessions.
- 🐧 **Multi-Distro Packaging**: Ready for Arch Linux (AUR), Fedora/RHEL (RPM), Debian/Ubuntu (.deb), and Flatpak.

---

## Architecture

```
+--------------------------------------------------------------+
|                     Desktop Session                          |
|  (KDE Plasma, GNOME, Sway, Hyprland, XFCE, i3, etc.)         |
+--------------------------------------------------------------+
                               |
                   [Super + .] Global Hotkey
                               v
+--------------------------------------------------------------+
|                        TrayHost                              |
|           (System Tray, SingleInstance LocalSocket)          |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                       EmojiPanel                             |
|  - Modern rounded floating UI (Frameless & Non-activating)   |
|  - QListView grid + EmojiDelegate with font fallback         |
|  - Standard Unicode category tabs (Smileys, People, etc.)   |
|  - Fast keyword search & Recent/Frequent tracking           |
+--------------------------------------------------------------+
                               | (Emoji Clicked)
                               v
+--------------------------------------------------------------+
|                     EmojiInjector                            |
|                                                              |
|   Wayland Session:                    X11 Session:           |
|   1. wtype direct injection           1. XTest keystrokes    |
|   2. wl-copy + Ctrl+V simulation      2. xclip + Ctrl+V      |
+--------------------------------------------------------------+
                               |
                               v
+--------------------------------------------------------------+
|                     Target Application                       |
|          (Firefox, Chrome, VSCode, Terminal, etc.)           |
+--------------------------------------------------------------+
```

---

## Installation & Building

### Prerequisites

- **CMake ≥ 3.25** and **Ninja**
- **Qt 6.5+** (`Core`, `Gui`, `Widgets`, `Network`, `Concurrent`, `DBus`)
- **C++23 compatible compiler** (`GCC 12+` or `Clang 15+`)
- **X11 Libraries**: `libX11`, `libXtst`
- **KDE Frameworks 6**: `KF6GlobalAccel`, `KF6XmlGui`
- **Recommended runtime helpers**:
  - Wayland: `wl-clipboard`, `wtype`
  - X11: `xclip`
  - Emoji Font: `noto-fonts-emoji` (or `fonts-noto-color-emoji`)

### Quick Install Dependencies

**Arch Linux / Manjaro:**
```bash
sudo pacman -S --needed cmake ninja gcc qt6-base qt6-tools kglobalaccel kxmlgui libxtst libx11 noto-fonts-emoji wl-clipboard wtype
```

**Fedora / RHEL:**
```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qttools-devel kf6-kglobalaccel-devel kf6-kxmlgui-devel libX11-devel libXtst-devel google-noto-color-emoji-fonts wl-clipboard
```

**Debian / Ubuntu (24.04+):**
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-tools-dev libkf6globalaccel-dev libkf6xmlgui-dev libx11-dev libxtst-dev fonts-noto-color-emoji wl-clipboard
```

### Build from Source

```bash
git clone https://github.com/Ayan-Jain-737/TypEmoji.git
cd TypEmoji

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build --parallel
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

### Run Locally

```bash
./build/bin/EmojiTyper
```

---

## System Integration & Autostart

### 1. Install System-Wide
```bash
sudo cmake --install build
```
This installs:
- Binary to `/usr/local/bin/EmojiTyper`
- Unicode data to `/usr/local/share/emojityper/data/`
- Desktop entry to `/usr/local/share/applications/org.emojiboard.EmojiTyper.desktop`
- SVG icon to `/usr/local/share/icons/hicolor/scalable/apps/emojityper.svg`

### 2. Autostart on Login (systemd user unit)
```bash
systemctl --user enable --now emojityper.service
```

Alternatively, copy the desktop entry into your XDG autostart folder:
```bash
mkdir -p ~/.config/autostart
cp org.emojiboard.EmojiTyper.desktop ~/.config/autostart/
```

### 3. Desktop Shortcuts

- **KDE Plasma**: The app registers with `KGlobalAccel` automatically for `Meta+.` (`Super+.`). You can also configure it in **System Settings → Shortcuts → EmojiTyper**.
- **GNOME**: Open **Settings → Keyboard → View and Customize Shortcuts → Custom Shortcuts**, add:
  - Name: `EmojiTyper`
  - Command: `EmojiTyper`
  - Shortcut: `Super + .`
- **Sway / Hyprland / i3**: Add to your compositor configuration:
  ```
  bindsym $mod+period exec EmojiTyper
  ```

---

## Packaging

Distribution packaging files are located in `packaging/`:
- **Arch Linux / AUR**: `packaging/PKGBUILD`
- **Fedora / RHEL / openSUSE**: `packaging/emojityper.spec`
- **Debian / Ubuntu**: `packaging/debian/`
- **Flatpak**: `packaging/org.emojiboard.EmojiTyper.yml`

To generate a portable tarball via CPack:
```bash
cd build
cpack -G TGZ
```

---

## License

EmojiTyper is licensed under the [MIT License](LICENSE).
Unicode emoji datasets are provided according to the [Unicode Terms of Use](https://www.unicode.org/copyright.html).