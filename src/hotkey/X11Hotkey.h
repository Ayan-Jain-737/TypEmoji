/**
 * X11Hotkey - Low-level X11 global hotkey implementation
 * Pure C++/X11 - NO Qt dependencies
 * This header can be included anywhere
 */
#pragma once

#include <cstdint>

// X11 modifier masks (defined here to avoid including X11 headers in Qt code)
#ifndef ShiftMask
#define ShiftMask    (1<<0)
#endif
#ifndef ControlMask
#define ControlMask  (1<<2)
#endif
#ifndef Mod1Mask
#define Mod1Mask     (1<<3)
#endif
#ifndef Mod4Mask
#define Mod4Mask     (1<<6)
#endif

// X11 key event types
#define KeyPressMask   (1L<<0)
#define KeyReleaseMask (1L<<1)

class X11Hotkey
{
public:
    struct HotkeyEvent {
        enum Type { Press, Release } type;
        using KeySymbol = unsigned long;
        KeySymbol keysym = 0;
    };

    X11Hotkey();
    ~X11Hotkey();

    // Non-copyable, movable
    X11Hotkey(const X11Hotkey&) = delete;
    X11Hotkey& operator=(const X11Hotkey&) = delete;
    X11Hotkey(X11Hotkey&&) = default;
    X11Hotkey& operator=(X11Hotkey&&) = default;

    // Install hotkey grab on X11 root window
    // keyCode: X11 keycode (e.g., XK_period)
    // modifiers: bitmask of ShiftMask, ControlMask, Mod1Mask, Mod4Mask
    bool install(int keyCode, unsigned int modifiers);

    // Uninstall hotkey grab
    void uninstall();

    // Poll for hotkey events (non-blocking)
    // Returns true if an event was retrieved
    bool pollEvent(HotkeyEvent& event);

    bool isInstalled() const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};