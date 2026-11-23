## TypEmoji v2.3.2 — Duplicate Startup Tray Icon Resolution & Single-Instance Process Mutex

TypEmoji v2.3.2 fixes an issue where multiple tray icons could appear upon desktop login, standardizes on standard XDG autostart specification, and adds file-level mutual exclusion (`QLockFile`) to prevent race conditions during parallel autostart triggers.

---

### What's Changed & Fixed in v2.3.2

#### 1. Eliminated Duplicate Startup Tray Icon on Login
- **Autostart Conflict Resolution**: Previous installer configurations registered both an XDG autostart `.desktop` entry (`~/.config/autostart/org.emojiboard.EmojiTyper.desktop`) and a systemd user unit (`~/.config/systemd/user/emojityper.service`). On desktop environments with systemd session integration (such as KDE Plasma and GNOME), both mechanisms would trigger upon login, causing two instances to launch simultaneously.
- **Installer Cleanup**: Removed redundant systemd service enablement from `install.sh`, `scripts/install.sh`, `scripts/create-installer.sh`, and `packaging/debian/postinst`.
- **Automatic Migration**: Installers and upgrade scripts now automatically check for, disable, and clean up any legacy user-level `emojityper.service` units.

#### 2. Process Mutex & Race Condition Prevention (`QLockFile`)
- **Microsecond Concurrency Protection**: Added `QLockFile` (`/tmp/EmojiTyperInstance.lock`) inside `SingleInstance` to guard against race conditions where two processes are spawned at the exact same millisecond before local IPC sockets bind.
- **Immediate Focus Delegation**: If a secondary instance is launched (either manually from the app menu, hotkey, or duplicate autostart), it immediately yields without initializing a tray icon or duplicate window, delegating focus to the active primary instance with a clean zero exit status.

---

### Installation & Downloads

#### One-Command Quick Install
```bash
curl -fsSL https://raw.githubusercontent.com/Ayan-Jain-737/TypEmoji/main/install.sh | bash
```

#### Precompiled Native Packages
| Distribution | Install Command / Download |
| :--- | :--- |
| **Arch Linux / Manjaro** | `sudo pacman -U https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.2/TypEmoji-2.3.2-Arch-Linux-x86_64.pkg.tar.zst` |
| **Ubuntu / Debian / Mint** | `sudo apt install ./TypEmoji-2.3.2-Ubuntu-Debian-x86_64.deb` |
| **Fedora / openSUSE / RHEL** | `sudo dnf install https://github.com/Ayan-Jain-737/TypEmoji/releases/download/v2.3.2/TypEmoji-2.3.2-Fedora-openSUSE-x86_64.rpm` |
| **Universal Linux (.run)** | `chmod +x TypEmoji-2.3.2-Universal-Installer.run && ./TypEmoji-2.3.2-Universal-Installer.run` |
| **Portable Tarball** | `tar -xzf TypEmoji-2.3.2-Portable-Linux-x86_64.tar.gz && ./TypEmoji/bin/EmojiTyper` |

---

🎮 **Try the live web demo**: [https://ayan-jain-737.github.io/TypEmoji/](https://ayan-jain-737.github.io/TypEmoji/)
