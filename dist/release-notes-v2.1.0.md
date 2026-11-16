## TypEmoji v2.1.0 — Visual Redesign & UX Polish

TypEmoji v2.1.0 delivers a complete visual redesign and modern desktop user experience inspired by native Linux and modern desktop guidelines.

### What's New in v2.1.0
- **Spacious Geometry**: Expanded panel to 396×506px with 12px outer drop shadow margins and concentric 14px rounded corners.
- **Vector Search Pill**: Replaced raw text emoji icon with an antialiased vector magnifying glass, sleek frosted input pill, and circular hover close button (`×`).
- **Pill Category Bar**: Redesigned tab bar with smooth pill-shaped category tabs, active accent indicator, and 38px touch targets.
- **Grid Polish & Anti-Jitter**: 44×44px concentric item cells with 22pt emoji typography; eliminated text shift on hover, and added a distinct amber badge for skin tone variants.
- **Thin Overlay Scrollbars**: Minimalist 6px rounded scrollbars that integrate cleanly into dark and light themes without clunky OS scrollbars.
- **Elevated Preview Bar**: 52px footer with a dedicated rounded preview tile, bold typography, shortcode and formatted Unicode codepoint pill tag (`U+1F600`), and keycap shortcut badges (`[↵ Insert]` `[⇧↵ Keep]`).
- **Screen Edge Clamping**: Added 12px safety margin to screen bounds so floating shadows remain completely visible near display edges.
- **Qt 6.4+ Compatibility**: Support for Qt 6.4 (Ubuntu 24.04, Debian 12) through Qt 6.12+ (Arch, Fedora 40+).

### Precompiled Packages
- **Universal Linux Installer (`.run`)**:
  ```bash
  chmod +x TypEmoji-2.1.0-Universal-Installer.run
  ./TypEmoji-2.1.0-Universal-Installer.run
  ```
- **Ubuntu / Debian / Linux Mint (`.deb`)**:
  ```bash
  sudo apt install ./TypEmoji-2.1.0-Ubuntu-Debian-x86_64.deb
  ```
- **Fedora / openSUSE / RHEL (`.rpm`)**:
  ```bash
  sudo dnf install ./TypEmoji-2.1.0-Fedora-openSUSE-x86_64.rpm
  ```
- **Arch Linux / Manjaro (`.pkg.tar.zst`)**:
  ```bash
  sudo pacman -U ./TypEmoji-2.1.0-Arch-Linux-x86_64.pkg.tar.zst
  ```
- **Portable Tarball (`.tar.gz`)**:
  ```bash
  tar -xzf TypEmoji-2.1.0-Portable-Linux-x86_64.tar.gz
  ./TypEmoji/bin/EmojiTyper
  ```
