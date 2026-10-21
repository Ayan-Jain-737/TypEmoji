# Hotkey Replacement Guide (Linux)

## Replacing Native Emoji Selectors

Different desktop environments have different native emoji pickers and hotkeys:

| Desktop | Native Hotkey | Native Picker |
|---------|---------------|---------------|
| GNOME | `Super+.` / `Ctrl+Shift+E` | GNOME Emoji Picker |
| KDE Plasma | `Meta+.` | KRunner emoji / Plasma Emoji |
| Sway/Hyprland | Custom | None built-in |
| XFCE | Custom | None built-in |

Our approach: **Intercept at X11 level via XGrabKey** so we capture the hotkey before the compositor/desktop handles it.

## Method 1: XGrabKey Interception (Implemented)

Our `GlobalHotkey` class uses `XGrabKey` on the X11 root window:
- Hook installed at app start
- Captures keydown event for configured combination (default: Mod4+Period = Super+.)
- Returns event to prevent further processing by compositor
- Our panel opens instead

This works on:
- X11 sessions (native)
- Xwayland applications (most apps on Wayland)
- KDE Plasma, GNOME on X11, XFCE, i3, etc.

```cpp
// XGrabKey captures the key before window manager/compositor
XGrabKey(display, keycode, modifiers, root, True,
         GrabModeAsync, GrabModeAsync);
```

## Method 2: Disable Native Picker via Desktop Settings

### GNOME
```bash
# Disable GNOME emoji picker hotkey
gsettings set org.gnome.desktop.interface gtk-enable-emoji-completion false

# Or change the hotkey
gsettings set org.gnome.settings-daemon.plugins.media-keys emoji-picker "[]"
```

### KDE Plasma
```bash
# Disable KRunner emoji via kglobalaccel
kwriteconfig5 --file kglobalaccelrc --group "krunner" --key "emoji" "none"

# Or use System Settings → Shortcuts → KRunner → Emoji → Disable
```

### Sway/Hyprland
Usually no built-in emoji picker - no conflict.

## Method 3: Wayland Native Hotkey (Future)

For full Wayland native support without Xwayland:

### KDE Plasma (kglobalacceld)
```bash
# Register via DBus
qdbus org.kde.kglobalaccel /kglobalaccel org.kde.kglobalaccel.setShortcut \
    "EmojiTyper" "show-panel" "Meta+."
```

### GNOME (media-keys)
```bash
# Register via DBus
gdbus call --session \
  --dest org.gnome.SettingsDaemon.MediaKeys \
  --object-path /org/gnome/SettingsDaemon/MediaKeys \
  --method org.gnome.SettingsDaemon.MediaKeys.SetCustomKeybinding \
  "EmojiTyper" "Show Emoji Panel" "Super+." "/path/to/emojityper"
```

### Generic (xdg-desktop-portal)
Use the GlobalShortcuts portal (requires user approval):
```bash
# Portal registration would be done via DBus at runtime
```

## Recommendation

**Use XGrabKey interception (Method 1)** as primary. It's:
- Non-invasive (no system config changes)
- Reversible (just quit app)
- Works without admin/root
- Covers X11 + Xwayland (90%+ of Linux apps)

Add Method 2 as optional "Disable native picker" button in Settings UI for users who want complete replacement.

## Changing Default Hotkey

Users can change hotkey in Settings (coming soon):
- Default: `Super + .` (Mod4Mask + XK_period)
- Customizable: Any modifier combination + key
- Modifier bitmask:
  - 1 = Shift
  - 2 = Ctrl
  - 4 = Alt
  - 8 = Super/Win/Meta (default)
- Config saved to `~/.config/EmojiTyper/settings.json`

## Testing Hotkey Capture

```bash
# Verify XGrabKey works
xev -root | grep -A2 "KeyPress"

# Press Super+. and verify our app receives it before desktop
```

## Known Conflicts

| App/DE | Conflict | Resolution |
|--------|----------|------------|
| GNOME on X11 | Super+. → GNOME picker | XGrabKey wins (our app starts first) |
| KDE Plasma | Meta+. → KRunner | XGrabKey wins |
| VS Code (Xwayland) | Super+. → VS Code command palette | Our app intercepts at X level |
| Firefox (Xwayland) | Super+. → Firefox emoji | Our app intercepts at X level |
| Native Wayland apps | No X11 event | Falls back to clipboard; future: portal |