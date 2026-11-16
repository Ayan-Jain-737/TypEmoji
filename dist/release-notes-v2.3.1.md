## TypEmoji v2.3.1 — Multi-Emoji Continuous Typing Fix & Dedicated Pin Mode

TypEmoji v2.3.1 brings critical stability and workflow improvements to multi-emoji typing on Linux (Wayland & X11), introducing a dedicated Header Pin Button (`📌`) alongside continuous typing fixups for `Shift + Click` and `Shift + Enter`.

---

### What's New & Fixed in v2.3.1

#### 1. Multi-Emoji Continuous Typing Fix (`Shift + Click` & `Shift + Enter`)
- **Activation Loss Protection**: In previous builds, rapid multi-emoji typing could cause the panel to inadvertently lose focus or auto-dismiss during asynchronous clipboard paste operations. Added `m_keepOpenInProgress` tracking and guarded `changeEvent(QEvent::ActivationChange)` with an automatic safety release timer.
- **Seamless Focus Retention**: After injecting an emoji with `Shift + Click`, focus smoothly returns to the emoji panel, allowing uninterrupted continuous selection of multiple emojis in one session.

#### 2. Dedicated Header Pin Button (`📌`)
- Added a native **Pin / Multi-Emoji Mode** button in the header bar next to the close button (`#pinBtn`).
- Clicking the Pin button toggles continuous multi-emoji insertion mode on or off without needing to hold down `Shift`.
- Active state displays an illuminated accent glow (`#0078D4`) indicating multi-selection mode is active.
- Fully mirrored in the interactive web simulator.

#### 3. KWin Wayland Script Collision Prevention
- Replaced timestamp-only KWin D-Bus script naming with an atomic increment sequence (`std::atomic<uint64_t> s_scriptSeq`).
- Eliminates D-Bus script registration collisions when pasting multiple emojis in rapid succession on KDE Plasma Wayland.

#### 4. Target Window State Preservation
- Improved target window handle and Wayland context caching across consecutive pastes, ensuring characters land reliably in the originating window even during fast multi-emoji bursts.

---

### Installation & Downloads

#### One-Command Quick Install
```bash
curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash
```

#### Precompiled Native Packages
| Distribution | Install Command / Download |
| :--- | :--- |
| **Arch Linux / Manjaro** | `sudo pacman -U https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.1/TypEmoji-2.3.1-Arch-Linux-x86_64.pkg.tar.zst` |
| **Ubuntu / Debian / Mint** | `sudo apt install ./TypEmoji-2.3.1-Ubuntu-Debian-x86_64.deb` |
| **Fedora / openSUSE / RHEL** | `sudo dnf install https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.1/TypEmoji-2.3.1-Fedora-openSUSE-x86_64.rpm` |
| **Universal Linux (.run)** | `chmod +x TypEmoji-2.3.1-Universal-Installer.run && ./TypEmoji-2.3.1-Universal-Installer.run` |
| **Portable Tarball** | `tar -xzf TypEmoji-2.3.1-Portable-Linux-x86_64.tar.gz && ./TypEmoji/bin/EmojiTyper` |

---

🎮 **Try the live web demo**: [https://ayan-jain-737.github.io/TypEmoji/](https://ayan-jain-737.github.io/TypEmoji/)
