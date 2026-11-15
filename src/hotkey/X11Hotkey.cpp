/**
 * X11Hotkey - Low-level X11 global hotkey implementation
 * This file MUST be compiled with X11 headers BEFORE Qt headers
 * It has NO Qt dependencies - pure C++/X11
 */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include "X11Hotkey.h"
#include <cstdio>

struct X11Hotkey::Impl
{
    Display* display = nullptr;
    Window root = 0;
    int keyCode = 0;
    unsigned int modifiers = 0;
    bool installed = false;

    static constexpr unsigned int LockMasks[] = {
        0,
        Mod2Mask,              // NumLock
        LockMask,              // CapsLock
        Mod2Mask | LockMask    // NumLock + CapsLock
    };

    ~Impl() {
        if (display) {
            if (installed) {
                for (unsigned int mask : LockMasks) {
                    XUngrabKey(display, keyCode, modifiers | mask, root);
                }
            }
            XCloseDisplay(display);
        }
    }
};

X11Hotkey::X11Hotkey() : m_impl(new Impl) {}

X11Hotkey::~X11Hotkey() {
    delete m_impl;
}

bool X11Hotkey::install(int keyCode, unsigned int modifiers)
{
    if (m_impl->installed) return true;

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        return false;
    }

    m_impl->display = display;
    m_impl->root = DefaultRootWindow(display);
    m_impl->keyCode = keyCode;
    m_impl->modifiers = modifiers;

    // Select input on root window to receive key events
    XSelectInput(display, m_impl->root, KeyPressMask | KeyReleaseMask);

    // Grab key with all NumLock/CapsLock variations
    bool anySuccess = false;
    for (unsigned int mask : Impl::LockMasks) {
        int grabResult = XGrabKey(display, keyCode, modifiers | mask, m_impl->root, True,
                                  GrabModeAsync, GrabModeAsync);
        if (grabResult == GrabSuccess) {
            anySuccess = true;
        }
    }

    if (!anySuccess) {
        XCloseDisplay(display);
        m_impl->display = nullptr;
        return false;
    }

    XFlush(display);
    m_impl->installed = true;
    return true;
}

void X11Hotkey::uninstall()
{
    if (!m_impl->installed) return;

    if (m_impl->display) {
        for (unsigned int mask : Impl::LockMasks) {
            XUngrabKey(m_impl->display, m_impl->keyCode, m_impl->modifiers | mask, m_impl->root);
        }
    }

    m_impl->installed = false;
}

bool X11Hotkey::pollEvent(HotkeyEvent& event)
{
    if (!m_impl->installed || !m_impl->display) return false;

    // Check for pending events without blocking
    if (XPending(m_impl->display) == 0) return false;

    XEvent xevent;
    XNextEvent(m_impl->display, &xevent);

    if (xevent.type == KeyPress || xevent.type == KeyRelease) {
        XKeyEvent* key = reinterpret_cast<XKeyEvent*>(&xevent);
        KeySym keysym = XLookupKeysym(key, 0);
        if (keysym == static_cast<KeySym>(m_impl->keyCode)) {
            event.type = (xevent.type == KeyPress) ? HotkeyEvent::Press : HotkeyEvent::Release;
            event.keysym = static_cast<HotkeyEvent::KeySymbol>(keysym);
            return true;
        }
    }
    return false;
}