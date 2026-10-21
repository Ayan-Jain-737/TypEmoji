/**
 * GlobalHotkey - Qt wrapper around X11Hotkey
 * Uses QTimer to poll X11 events
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QTimer>
#include <memory>
#include "X11Hotkey.h"

class GlobalHotkey : public QObject
{
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;

    bool install();
    void uninstall();

    void setHotkey(int modifiers, int keyCode);
    int modifiers() const { return m_modifiers; }
    int keyCode() const { return m_keyCode; }

signals:
    void hotkeyPressed(const QPoint& caretPos);
    void hotkeyReleased();

private slots:
    void pollX11Events();

private:
    std::unique_ptr<X11Hotkey> m_x11Hotkey;
    QTimer m_pollTimer;

    int m_modifiers = 0;
    int m_keyCode = 0x2e;  // XK_period
};