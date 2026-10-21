# EmojiTyper Task Plan (Linux X11 & Wayland)

## Project Goal
Direct emoji typing tool for Linux (X11 & Wayland) that types emojis directly into focused text fields on click, rivaling KDE Plasma EmojiSelector and Windows Emoji Panel.

## Phases

### Phase 0: Critical Bug Fixes (Completed)
- [x] Remove 5-second auto-close timer (`EmojiPanel.cpp`)
- [x] Implement click-outside-to-close behavior (`EmojiPanel.cpp`)
- [x] Fix category ordering to standard Unicode order (`EmojiDatabase.cpp`)
- [x] Fix tab-to-category mapping and "ocoa" flag typo (`EmojiPanel.cpp`)
- [x] Filter out "Component" and skin-tone flooding (`EmojiDatabase.cpp`)
- [x] Multi-backend injection pipeline for Wayland (`wl-copy` + `wtype`) and X11 (`xclip` + `XTest`) (`EmojiInjector.cpp`)
- [x] Clean up debug cerr logging across codebase

### Phase 1: UI/UX Parity (Completed)
- [x] Non-activating frameless window with rounded styling
- [x] EmojiDelegate with font fallbacks (Noto Color Emoji, Segoe UI Emoji)
- [x] Search filtering and category navigation
- [x] Dynamic Recent and Frequent tracking

### Phase 2: Desktop Integration & Hotkey (Completed)
- [x] KDE KGlobalAccel integration
- [x] X11 XGrabKey fallback
- [x] Single instance toggle IPC (`SingleInstance.cpp`)
- [x] Desktop entry (`org.emojiboard.EmojiTyper.desktop`)
- [x] Systemd user service (`installer/emojityper.service`)
- [x] SVG application icon (`assets/icons/emojityper.svg`)

### Phase 3: Multi-Distro Packaging (Completed)
- [x] Arch Linux / AUR (`packaging/PKGBUILD`)
- [x] Fedora / RHEL / openSUSE (`packaging/emojityper.spec`)
- [x] Debian / Ubuntu (`packaging/debian/`)
- [x] Flatpak manifest (`packaging/org.emojiboard.EmojiTyper.yml`)

### Phase 4: Open Source Readiness (Completed)
- [x] Full MIT License (`LICENSE`)
- [x] `CONTRIBUTING.md`
- [x] `CODE_OF_CONDUCT.md`
- [x] Polished `README.md`