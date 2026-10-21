/**
 * GlobalHotkey implementation - Qt wrapper around X11Hotkey
 */
#include "GlobalHotkey.h"
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent)
{
    m_x11Hotkey = std::make_unique<X11Hotkey>();
    m_pollTimer.setInterval(10); // Poll every 10ms
    connect(&m_pollTimer, &QTimer::timeout, this, &GlobalHotkey::pollX11Events);
}

GlobalHotkey::~GlobalHotkey()
{
    uninstall();
}

bool GlobalHotkey::install()
{
    if (!m_x11Hotkey) return false;

    // Convert our modifier bitmask to X11 modifiers
    unsigned int xmodifiers = 0;
    if (m_modifiers & 0x01) xmodifiers |= ShiftMask;
    if (m_modifiers & 0x02) xmodifiers |= ControlMask;
    if (m_modifiers & 0x04) xmodifiers |= Mod1Mask;
    if (m_modifiers & 0x08) xmodifiers |= Mod4Mask;

    if (!m_x11Hotkey->install(m_keyCode, xmodifiers)) {
        qWarning() << "Failed to install X11 hotkey";
        return false;
    }

    m_pollTimer.start();
    qDebug() << "GlobalHotkey installed";
    return true;
}

void GlobalHotkey::uninstall()
{
    m_pollTimer.stop();
    if (m_x11Hotkey) {
        m_x11Hotkey->uninstall();
    }
    qDebug() << "GlobalHotkey uninstalled";
}

void GlobalHotkey::setHotkey(int modifiers, int keyCode)
{
    m_modifiers = modifiers;
    m_keyCode = keyCode;
}

void GlobalHotkey::pollX11Events()
{
    if (!m_x11Hotkey) return;

    X11Hotkey::HotkeyEvent event;
    while (m_x11Hotkey->pollEvent(event)) {
        if (event.type == X11Hotkey::HotkeyEvent::Press) {
            emit hotkeyPressed(QPoint());
        } else {
            emit hotkeyReleased();
        }
    }
}