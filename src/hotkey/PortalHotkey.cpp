/**
 * PortalHotkey - Implementation using XDG Desktop Portal GlobalShortcuts
 */
#include "PortalHotkey.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QVariantMap>

static const char* PORTAL_SERVICE = "org.freedesktop.portal.Desktop";
static const char* PORTAL_PATH = "/org/freedesktop/portal/desktop";
static const char* PORTAL_IFACE = "org.freedesktop.portal.GlobalShortcuts";

PortalHotkey::PortalHotkey(QObject* parent) : QObject(parent)
{
}

PortalHotkey::~PortalHotkey()
{
    uninstall();
}

bool PortalHotkey::isAvailable() const
{
    QDBusInterface iface(PORTAL_SERVICE, PORTAL_PATH, PORTAL_IFACE, QDBusConnection::sessionBus());
    return iface.isValid();
}

bool PortalHotkey::install()
{
    if (m_installed) return true;

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return false;

    QDBusInterface iface(PORTAL_SERVICE, PORTAL_PATH, PORTAL_IFACE, bus);
    if (!iface.isValid()) {
        qDebug() << "PortalHotkey: XDG Desktop Portal GlobalShortcuts interface not available";
        return false;
    }

    // Connect to Activated signal
    bus.connect(
        PORTAL_SERVICE,
        QString(), // Match any path
        PORTAL_IFACE,
        "Activated",
        this,
        SLOT(onPortalSignal(QDBusMessage))
    );

    // CreateSession options
    QVariantMap sessionOpts;
    sessionOpts["session_handle_token"] = QString("emojityper_%1").arg(QDateTime::currentSecsSinceEpoch());
    sessionOpts["handle_token"] = QString("req_%1").arg(QDateTime::currentSecsSinceEpoch());

    QDBusMessage reply = iface.call("CreateSession", sessionOpts);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qDebug() << "PortalHotkey: CreateSession failed:" << reply.errorMessage();
        return false;
    }

    m_installed = true;
    qDebug() << "PortalHotkey: listening for portal global shortcut signals";
    return true;
}

void PortalHotkey::uninstall()
{
    if (!m_installed) return;
    QDBusConnection::sessionBus().disconnect(
        PORTAL_SERVICE,
        QString(),
        PORTAL_IFACE,
        "Activated",
        this,
        SLOT(onPortalSignal(QDBusMessage))
    );
    m_installed = false;
}

void PortalHotkey::onPortalSignal(const QDBusMessage& message)
{
    // Activated signal: (session_handle, shortcut_id, timestamp, options)
    QList<QVariant> args = message.arguments();
    if (args.size() >= 2) {
        QString shortcutId = args.at(1).toString();
        if (shortcutId.contains("emojityper") || shortcutId.contains("emoji") || shortcutId.contains("show")) {
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastTriggerTime < 350) return;
            m_lastTriggerTime = now;
            emit hotkeyPressed(QPoint());
        }
    }
}
