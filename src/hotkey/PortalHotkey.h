/**
 * PortalHotkey - Global hotkey implementation using XDG Desktop Portal
 * Works on GNOME 44+, Sway, Hyprland, Cosmic, and standard Wayland desktops.
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QPoint>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

class PortalHotkey : public QObject
{
    Q_OBJECT
public:
    explicit PortalHotkey(QObject* parent = nullptr);
    ~PortalHotkey() override;

    bool install();
    void uninstall();
    bool isAvailable() const;

signals:
    void hotkeyPressed(const QPoint& caretPos);

private slots:
    void onPortalSignal(const QDBusMessage& message);

private:
    QString m_sessionHandle;
    bool m_installed = false;
    qint64 m_lastTriggerTime = 0;
};
