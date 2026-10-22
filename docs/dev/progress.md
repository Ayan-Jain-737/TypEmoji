# Progress Log

## Session 2026-10-21

### Completed Tasks

1. **Panel Persistence & Dismiss Behavior**:
   - Removed the premature window closure on emoji click: the panel now remains open so users can click multiple emojis consecutively (e.g. "😀🎉🔥") into their target text box without having to reopen the app.
   - Implemented click-outside detection via `m_clickOutsideTimer` polling `QGuiApplication::mouseButtons()`: as soon as the user clicks anywhere outside the panel boundaries, the panel dismisses immediately.
   - Updated keyboard handling: pressing Escape or any non-navigation key when not searching dismisses the panel so normal document typing can immediately resume.

2. **Direct Typing & Injection Pipeline**:
   - Configured non-activating focus policies across all picker widgets (`QListView`, `QTabBar`, `QPushButton` all set to `Qt::NoFocus`), preventing the picker from stealing keyboard focus from the active text box during clicks.
   - Native Wayland injection: executes hardware-level evdev `Ctrl+V` key events via `ydotool key 29:1 47:1 47:0 29:0`, delivering real keystrokes to any active window.
   - Preserved clipboard safety: multi-paste timer resets on each click, ensuring that rapid emoji insertions do not overwrite the user's original clipboard content until 1.2s of inactivity after the session completes.

3. **Categorization & UI Improvements**:
   - Replaced alphabetical category sorting in `EmojiDatabase.cpp` with the standard Unicode Emoji order (Smileys & Emotion, People & Body, Animals & Nature, etc.).
   - Filtered out spurious "Component" groups and skin-tone clutter from the primary category grids.
   - Fixed the "ocoa" typo on the Flags tab in `EmojiPanel.cpp`.
   - Hardcoded `CategoryDef` structs in `EmojiPanel.h` and `EmojiPanel.cpp` ensuring exact 1-to-1 tab-to-view mapping.
   - Updated Recent & Frequent tab handling to dynamically refresh on selection.

4. **Desktop Integration & Hotkey System**:
   - Added standard desktop file `org.emojiboard.EmojiTyper.desktop` with KDE shortcut hints and categories.
   - Created modern SVG vector icon in `assets/icons/emojityper.svg`.
   - Created systemd user service `installer/emojityper.service` for autostart across distros.
   - Verified single-instance IPC (`QLocalServer` / `QLocalSocket`) with toggle behavior so pressing the keybind or running the command brings the panel up or toggles it away.

5. **Multi-Distro Packaging & Open-Source Readiness**:
   - Arch Linux / AUR: `packaging/PKGBUILD`
   - Fedora / RHEL / openSUSE: `packaging/emojityper.spec`
   - Debian / Ubuntu: `packaging/debian/control`, `rules`, `changelog`
   - Flatpak: `packaging/org.emojiboard.EmojiTyper.yml`
   - Completed standard MIT `LICENSE`.
   - Added `CONTRIBUTING.md` and `CODE_OF_CONDUCT.md`.
   - Rewrote `README.md` with complete documentation, architecture diagram, and setup guides.

6. **Build & Test Verification**:
   - Clean compilation with GCC 14 / Qt 6.8 / C++23.
   - 100% unit tests passing (`ctest`).