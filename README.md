# TypEmoji — Direct Emoji Typing Tool for Linux (X11 & Wayland)

<p align="center">
  <img src="assets/icons/emojityper.svg" alt="TypEmoji Logo" width="104" height="104">
</p>

<p align="center">
  <b>A lightweight, high-performance, non-activating emoji picker and direct typing tool for Linux.</b><br>
  Types Unicode emojis directly into any focused window with <code>Meta + .</code> without touching your clipboard.<br>
  Built with C++23, Qt 6, and native Wayland/X11 protocols.
</p>

<p align="center">
  <a href="https://github.com/Ayan-Jain-737/TypEmoji/releases/latest"><img src="https://img.shields.io/github/v/release/Ayan-Jain-737/TypEmoji?style=flat-square&color=blue&label=Latest%20Release" alt="Latest Release"></a>
  <img src="https://img.shields.io/badge/Platform-Linux%20(X11%20%26%20Wayland)-orange?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Standard-Unicode%2016.0-green?style=flat-square" alt="Unicode 16.0">
  <img src="https://img.shields.io/badge/License-MIT-purple?style=flat-square" alt="License">
</p>

---

## ⚡ Why TypEmoji? (Comparison with Default Linux Pickers)

Most Linux desktop environments provide built-in emoji utilities, but they come with significant productivity hurdles. Here is how **TypEmoji** solves them:

| Feature | 🐧 GNOME Emoji (`Ctrl+.`) | 🔵 KDE Plasma (`plasma-emojier`) | 🐍 Emote / Python Tools | ✨ **TypEmoji** |
| :--- | :--- | :--- | :--- | :--- |
| **Direct Keystroke Typing** | ⚠️ GTK apps only | ❌ No (Copies to clipboard) | ⚠️ Emulates Ctrl+V | ✅ **Yes (Direct injection)** |
| **Preserves Clipboard** | ⚠️ Overwrites clipboard | ❌ Overwrites clipboard | ❌ Overwrites clipboard | ✅ **100% Preserved** |
| **Works Across All Apps** | ❌ Fails in Chrome/Electron/Alacritty | ⚠️ Inconsistent focus | ⚠️ Inconsistent focus | ✅ **Any window, editor, terminal** |
| **Focus Preservation** | ❌ Steals window focus | ❌ Steals window focus | ⚠️ Minor jitter | ✅ **True non-activating window** |
| **Global Shortcut** | ❌ Hardcoded to GTK fields | ⚠️ Clunky daemon | ⚠️ Requires manual binding | ✅ **Native `Meta + .` (Win + .)** |
| **Speed & Resource Usage** | 🐢 IBus dependency overhead | 🐢 Heavy QML process | 🐢 120MB+ RAM (Python/Electron)| ⚡ **Instant (~18MB RAM in C++23)** |
| **Unicode 16.0 Completeness**| ⚠️ Partial / Distro-dependent | ⚠️ Distro-dependent | ⚠️ Outdated databases | ✅ **Full 3,900+ emojis & ZWJs** |

### Key Advantages:
1. **Never Hijacks Your Clipboard**: The default KDE picker copies emojis to the clipboard, destroying whatever link, code snippet, or text you had copied. TypEmoji types the character directly into your target app via native virtual input events.
2. **True Focus-Free Floating Panel**: Other pickers steal focus, causing your web browser, VS Code, or terminal to drop the blinking cursor. TypEmoji uses `WA_ShowWithoutActivating` and `X11 BypassWindowManagerHint` so your target app remains focused.
3. **Works Everywhere**: Whether you are typing in Discord, VS Code, Google Chrome, Slack, Kitty, Alacritty, or LibreOffice, TypEmoji seamlessly inputs emojis everywhere on both X11 and Wayland.
4. **Instant Startup**: Written in modern C++23 and compiled to native machine code. It opens in sub-milliseconds without lag.

---

## 📥 Download & One-Click Install

Download the package tailored for your distribution from the **[Latest GitHub Releases](https://github.com/Ayan-Jain-737/TypEmoji/releases/latest)**:

### 🌟 1. Universal Self-Resolving Installer (Recommended for Any Distro)
*Works on **Ubuntu, Fedora, Arch, openSUSE, Linux Mint, Pop!_OS, Debian, and Manjaro**.*

1. Download **`TypEmoji-1.0.0-Universal-Installer.run`** (or `EmojiTyper-1.0.0-Universal-Installer.run`).
2. Right-click the file → **Properties** → check **"Allow executing file as program"** (or in terminal: `chmod +x *.run`).
3. Double-click the file to run it.
4. **Done!** The installer automatically:
   - Detects your Linux distribution.
   - Automatically installs any missing Qt6 or KF6 runtime libraries.
   - Installs desktop shortcuts and sets up autostart on system login.
   - Immediately starts the background daemon and claims the `Meta + .` global hotkey.

---

### 🟠 2. Ubuntu / Debian / Linux Mint / Pop!_OS (`.deb`)
1. Download **`TypEmoji-1.0.0-Ubuntu-Debian-x86_64.deb`**.
2. Double-click to install using **Ubuntu Software / App Center / GDebi**.
3. Or install via terminal (which automatically resolves all dependencies):
   ```bash
   sudo apt install ./EmojiTyper-1.0.0-Ubuntu-Debian-x86_64.deb
   ```

---

### 🔵 3. Arch Linux / Manjaro (`.pkg.tar.zst`)
1. Download **`TypEmoji-1.0.0-Arch-Linux-x86_64.pkg.tar.zst`**.
2. Install via `pacman`:
   ```bash
   sudo pacman -U EmojiTyper-1.0.0-Arch-Linux-x86_64.pkg.tar.zst
   ```

---

### 🔴 4. Fedora / RHEL / openSUSE (`.rpm`)
1. Download **`TypEmoji-1.0.0-Fedora-openSUSE-x86_64.rpm`**.
2. Double-click to install in **GNOME Software**, or run:
   ```bash
   sudo dnf install ./EmojiTyper-1.0.0-Fedora-openSUSE-x86_64.rpm
   ```

---

### ⚪ 5. Portable Archive (No Installation Required)
1. Download **`TypEmoji-1.0.0-Portable-Linux-x86_64.tar.gz`**.
2. Extract the archive and launch directly:
   ```bash
   tar -xzf EmojiTyper-1.0.0-Portable-Linux-x86_64.tar.gz
   ./EmojiTyper-1.0.0-Linux/bin/EmojiTyper
   ```

---

### ⚡ 6. Instant 1-Command Web Install
Paste this single command into any terminal to install TypEmoji system-wide:
```bash
curl -fsSL https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v1.0.0/EmojiTyper-1.0.0-Linux.tar.gz | sudo tar -xz -C /usr/local --strip-components=1 && sudo update-desktop-database
```

---

## ⌨️ How to Use

1. **Triggering the Panel**:
   * Press **`Meta + .`** (the Windows key + period) anywhere.
   * Or click the smiley icon in your **System Tray**.
2. **Searching**:
   * Type any keyword (e.g. `fire`, `rocket`, `cat`, `heart`, `laugh`) to instantly filter emojis.
   * Use **Arrow keys** to navigate and **Enter** to insert.
3. **Recent & Frequent Emojis**:
   * Frequently and recently used emojis are automatically remembered and displayed on the front tab for quick access.
4. **Category Navigation**:
   * Switch between standard Unicode categories (Smileys & Emotion, People, Animals & Nature, Food & Drink, Travel, Activities, Objects, Symbols, Flags) using mouse or tab shortcuts.

---

## 🏗️ Architecture & How It Works

```
+--------------------------------------------------------------+
|                     Desktop Session                          |
|  (KDE Plasma, GNOME, Sway, Hyprland, XFCE, i3, etc.)         |
+--------------------------------------------------------------+
                               |
                    [Meta + .] Global Hotkey
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
|  - Standard Unicode category tabs (Smileys, People, etc.)    |
|  - Sub-millisecond keyword search & Recent/Frequent tracking |
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
|          (Firefox, Chrome, VS Code, Discord, Terminal)       |
+--------------------------------------------------------------+
```

---

## 🛠️ Building from Source

If you prefer to compile TypEmoji yourself:

### 1. Install Build Dependencies
* **Ubuntu / Debian**:
  ```bash
  sudo apt install cmake ninja-build g++ qt6-base-dev qt6-tools-dev libkf6globalaccel-dev libkf6xmlgui-dev libx11-dev libxtst-dev fonts-noto-color-emoji
  ```
* **Fedora**:
  ```bash
  sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qttools-devel kf6-kglobalaccel-devel kf6-kxmlgui-devel libX11-devel libXtst-devel google-noto-color-emoji-fonts
  ```
* **Arch Linux**:
  ```bash
  sudo pacman -S --needed cmake ninja gcc qt6-base qt6-tools kglobalaccel kxmlgui libxtst libx11 noto-fonts-emoji
  ```

### 2. Compile & Install
```bash
git clone https://github.com/Ayan-Jain-737/TypEmoji.git
cd TypEmoji

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Install system-wide
sudo cmake --install build
```

---

## 📚 Documentation Reference

For deep technical insights and design specifications, refer to:
* [Architecture Deep Dive](docs/ARCHITECTURE.md)
* [Build Guide & Flags](docs/BUILD.md)
* [Global Hotkey & Desktop Integration](docs/HOTKEY_REPLACEMENT.md)
* [Unicode 16.0 Specification Compliance](docs/SPEC_COMPLIANCE.md)
* [Test & Validation Matrix](docs/validation.md)

---

## 📄 License

TypEmoji is licensed under the **[MIT License](LICENSE)**.  
Unicode emoji datasets are provided according to the **[Unicode Terms of Use](https://www.unicode.org/copyright.html)**.