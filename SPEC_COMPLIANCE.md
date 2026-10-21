# SPEC Compliance Matrix

This document maps the SPEC.md requirements to the Linux/X11/Wayland implementation.

## SPEC Requirements (from t_bdeba7bf)

| ID | Requirement | Status | Implementation Location |
|----|-------------|--------|------------------------|
| R1 | Global hotkey configurable | ✅ | `src/hotkey/GlobalHotkey.{h,cpp}` - XGrabKey on root window |
| R2 | Non-activating panel | ✅ | `src/panel/EmojiPanel.cpp` - Qt::Tool \| Qt::FramelessWindowHint \| Qt::WindowDoesNotAcceptFocus \| Qt::WindowStaysOnTopHint + WA_ShowWithoutActivating |
| R3 | Direct Unicode injection | ✅ | `src/injector/EmojiInjector.{h,cpp}` - wtype (Wayland) + XTest (X11) + clipboard fallback |
| R4 | Repeat insertion | ✅ | `EmojiPanel::onEmojiClicked` - panel stays open after click |
| R5 | Caret anchoring | ✅ | `TargetState::capture()` uses XGetInputFocus + XGetWindowAttributes |
| R6 | Focus restoration | ✅ | `TargetState::ensureForeground()` uses XRaiseWindow + XSetInputFocus |
| R7 | Emoji 17 data loading | ✅ | `src/data/EmojiDatabase.{h,cpp}` - parses emoji-test.txt + emoji-zwj-sequences.txt |
| R8 | Recent/frequent persistence | ✅ | `src/settings/Settings.{h,cpp}` - JSON in ~/.config/EmojiTyper/settings.json |
| R9 | Categories supported | ✅ | `EmojiDatabase::categories()` + `EmojiModel` category filter |
| R10 | Skin-tone picker | ⏳ Planned | `EmojiDatabase::skinToneVariants()` exists, UI pending |
| R11 | Keyboard navigation | ✅ | `EmojiPanel::keyPressEvent` - Escape, Enter, arrow keys via list view |
| R12 | Single instance IPC | ✅ | `src/ipc/SingleInstance.{h,cpp}` - QLocalServer/QLocalSocket |
| R13 | Tray host | ✅ | `src/tray/TrayHost.{h,cpp}` - QSystemTrayIcon |
| R14 | Per-user install | ✅ | Settings in ~/.config, autostart via .desktop file |
| R15 | Clean shutdown | ✅ | QObject parent hierarchy + tray quit handler |

## Platform-Specific Adaptations

| Windows API | Linux Replacement | Notes |
|-------------|-------------------|-------|
| `SendInput` + `KEYEVENTF_UNICODE` | `wtype` (Wayland) / `XTest` (X11) | Primary injection path |
| `WH_KEYBOARD_LL` hook | `XGrabKey` on root window | Global hotkey capture |
| `GetForegroundWindow` | `XGetInputFocus` | Target capture |
| `GetGUIThreadInfo` | X11 window attributes | Focused control approximation |
| `AttachThreadInput` | N/A (not needed) | Linux has no UIPI |
| `WM_COPYDATA` IPC | `QLocalServer`/`QLocalSocket` | Single instance |
| Named mutex | QLocalServer lock file | Instance detection |
| `WS_EX_NOACTIVATE` | Qt window flags | Non-activating panel |

## Acceptance Criteria Verification

| Test | Expected | Linux Implementation |
|------|----------|---------------------|
| Hotkey opens panel | Super+. opens panel | XGrabKey intercepts before compositor |
| Direct insert (gedit) | Click 😀 → "😀" at caret | wtype/XTest injects Unicode |
| Repeat insert | Click 😀 5× → "😀😀😀😀😀" | Panel stays open, re-injects |
| Caret anchoring | Panel at text cursor | XGetWindowAttributes center |
| No focus theft | Panel doesn't activate | WA_ShowWithoutActivating + Qt::WindowDoesNotAcceptFocus |
| Multi-emoji surrogate | 👨‍👩‍👧‍👦 inserts correctly | UTF-32 codepoint iteration |
| Search | Type "smile" → filters | EmojiDatabase keyword index |
| Tray/single instance | Double-launch → single panel | QLocalServer instance lock |
| Settings persist | Hotkey/theme survive reboot | ~/.config/EmojiTyper/settings.json |

## Known Limitations (Wayland)

1. **Global hotkey on Wayland**: XGrabKey only works on X11/Xwayland. For native Wayland:
   - Use `KGlobalAccel` (KDE) or `GSettings` (GNOME) with GlobalShortcuts portal
   - Or run Xwayland applications only

2. **Direct injection on Wayland**: `wtype` requires `text-input-v3` protocol support in compositor

3. **Caret position on Wayland**: No standard way to get caret position; falls back to window center

## Build Verification

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
ctest --output-on-failure
```