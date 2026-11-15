# TypEmoji Release Notes

## Summary
<!-- Brief 1-2 sentence summary of this release -->

## What's New
<!-- Major features and highlights -->

## Bug Fixes & Improvements
<!-- Specific bug fixes and usability enhancements -->

## Installation

### Precompiled Packages
- **Universal Linux Installer (`.run`)**:
  ```bash
  chmod +x TypEmoji-<version>-Universal-Installer.run
  ./TypEmoji-<version>-Universal-Installer.run
  ```
- **Ubuntu / Debian / Linux Mint (`.deb`)**:
  ```bash
  sudo apt install ./TypEmoji-<version>-Ubuntu-Debian-x86_64.deb
  ```
- **Fedora / openSUSE / RHEL (`.rpm`)**:
  ```bash
  sudo dnf install ./TypEmoji-<version>-Fedora-openSUSE-x86_64.rpm
  ```
- **Arch Linux / Manjaro (`.pkg.tar.zst`)**:
  ```bash
  sudo pacman -U ./TypEmoji-<version>-Arch-Linux-x86_64.pkg.tar.zst
  ```
- **Portable Tarball (`.tar.gz`)**:
  ```bash
  tar -xzf TypEmoji-<version>-Portable-Linux-x86_64.tar.gz
  ./TypEmoji/bin/EmojiTyper
  ```

### Flatpak
```bash
flatpak-builder --install --user build-dir packaging/org.emojiboard.EmojiTyper.flatpak.yml
```

## Full Changelog
See [CHANGELOG.md](CHANGELOG.md) for full historical release notes.
