/**
 * TargetState implementation - X11/Wayland target capture & focus restoration
 * High-performance implementation using native QtDBus (0ms process spawn overhead)
 */
#include "TargetState.h"
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtGui/QCursor>
#include <QtWidgets/QApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

TargetState::TargetState(QObject* parent) : QObject(parent)
{
    m_isWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
    // Register on session DBus so KWin helper scripts can call setTargetUuid()
    QDBusConnection::sessionBus().registerService("org.emojiboard.EmojiTyper");
    QDBusConnection::sessionBus().registerObject("/TargetState", this, QDBusConnection::ExportAllSlots);
}

void TargetState::setTargetUuid(const QString& uuid)
{
    m_targetUuid = uuid;
    qDebug() << "TargetState: setTargetUuid received:" << m_targetUuid;
}

bool TargetState::capture()
{
    if (m_isWayland) {
        if (captureKWinWayland()) {
            return true;
        }
    }
    return captureX11();
}

bool TargetState::captureKWinWayland()
{
    m_targetUuid.clear();
    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/emojityper_cap.js";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(
        "var act = workspace.activeWindow;\n"
        "callDBus('org.emojiboard.EmojiTyper', '/TargetState', 'org.emojiboard.TargetState', 'setTargetUuid', act ? String(act.internalId) : '');\n"
    );
    f.close();

    QString pluginName = QString("et_cap_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDBusInterface kwinScripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus());
    QDBusReply<int> reply = kwinScripting.call("loadScript", scriptPath, pluginName);
    if (reply.isValid() && reply.value() > 0) {
        int scriptId = reply.value();
        QDBusInterface scriptObj("org.kde.KWin", QString("/Scripting/Script%1").arg(scriptId), "org.kde.kwin.Script", QDBusConnection::sessionBus());
        scriptObj.call("run");

        for (int i = 0; i < 20 && m_targetUuid.isEmpty(); ++i) {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }

        kwinScripting.call("unloadScript", pluginName);
    }
    QFile::remove(scriptPath);

    m_caretScreenPos = QCursor::pos();
    m_valid = !m_targetUuid.isEmpty();
    qDebug() << "TargetState: captured KWin Wayland target UUID:" << m_targetUuid << "valid:" << m_valid;
    return m_valid;
}

bool TargetState::captureX11()
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        qWarning() << "Failed to open X display for target capture";
        return false;
    }

    Window root = DefaultRootWindow(display);
    Window focus;
    int revert_to;
    XGetInputFocus(display, &focus, &revert_to);

    if (focus == None || focus == root) {
        qWarning() << "No focused X11 window (focus: " << focus << ")";
        XCloseDisplay(display);
        return false;
    }

    m_targetWindow = focus;

    XWindowAttributes attr;
    if (XGetWindowAttributes(display, focus, &attr)) {
        m_caretScreenPos = QPoint(attr.x + attr.width / 2, attr.y + attr.height / 2);
    } else {
        m_caretScreenPos = QCursor::pos();
    }

    XCloseDisplay(display);
    m_valid = true;
    qDebug() << "Captured X11 target: window=" << m_targetWindow << "caret=" << m_caretScreenPos;
    return true;
}

bool TargetState::isValid() const
{
    if (!m_valid) return false;
    if (m_isWayland && !m_targetUuid.isEmpty()) return true;
    if (m_targetWindow == 0) return false;

    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    XWindowAttributes attr;
    bool ok = XGetWindowAttributes(display, m_targetWindow, &attr) != 0;
    XCloseDisplay(display);
    return ok;
}

bool TargetState::ensureForeground()
{
    if (m_isWayland && !m_targetUuid.isEmpty()) {
        if (activateKWinWayland()) {
            return true;
        }
    }
    return activateX11();
}

bool TargetState::activateKWinWayland()
{
    if (m_targetUuid.isEmpty()) return false;

    QString script = QString(
        "var clients = workspace.windowList();\n"
        "for (var i = 0; i < clients.length; i++) {\n"
        "    if (String(clients[i].internalId) === '%1') {\n"
        "        workspace.activeWindow = clients[i];\n"
        "        break;\n"
        "    }\n"
        "}\n"
    ).arg(m_targetUuid);

    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/emojityper_act.js";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(script.toUtf8());
    f.close();

    QString pluginName = QString("et_act_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDBusInterface kwinScripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus());
    QDBusReply<int> reply = kwinScripting.call("loadScript", scriptPath, pluginName);
    if (reply.isValid() && reply.value() > 0) {
        int scriptId = reply.value();
        QDBusInterface scriptObj("org.kde.KWin", QString("/Scripting/Script%1").arg(scriptId), "org.kde.kwin.Script", QDBusConnection::sessionBus());
        scriptObj.call("run");
        kwinScripting.call("unloadScript", pluginName);
    }
    QFile::remove(scriptPath);
    return true;
}

bool TargetState::ensurePanelAbove()
{
    if (!m_isWayland) return false;

    QString script =
        "var clients = workspace.windowList();\n"
        "for (var i = 0; i < clients.length; i++) {\n"
        "    var c = clients[i];\n"
        "    var cap = c.caption || '';\n"
        "    var res = c.resourceClass || '';\n"
        "    if (cap.indexOf('EmojiTyper') !== -1 || res.indexOf('EmojiTyper') !== -1 || res.indexOf('emojityper') !== -1) {\n"
        "        c.keepAbove = true;\n"
        "        c.skipTaskbar = true;\n"
        "        c.skipPager = true;\n"
        "    }\n"
        "}\n";

    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/emojityper_above.js";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(script.toUtf8());
    f.close();

    QString pluginName = QString("et_above_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDBusInterface kwinScripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus());
    QDBusReply<int> reply = kwinScripting.call("loadScript", scriptPath, pluginName);
    if (reply.isValid() && reply.value() > 0) {
        int scriptId = reply.value();
        QDBusInterface scriptObj("org.kde.KWin", QString("/Scripting/Script%1").arg(scriptId), "org.kde.kwin.Script", QDBusConnection::sessionBus());
        scriptObj.call("run");
        kwinScripting.call("unloadScript", pluginName);
    }
    QFile::remove(scriptPath);
    return true;
}

bool TargetState::activateEmojiPanel()
{
    if (!m_isWayland) return false;

    QString script =
        "var clients = workspace.windowList();\n"
        "for (var i = 0; i < clients.length; i++) {\n"
        "    var c = clients[i];\n"
        "    var cap = c.caption || '';\n"
        "    var res = c.resourceClass || '';\n"
        "    if (cap.indexOf('EmojiTyper') !== -1 || res.indexOf('EmojiTyper') !== -1 || res.indexOf('emojityper') !== -1) {\n"
        "        c.keepAbove = true;\n"
        "        workspace.activeWindow = c;\n"
        "        break;\n"
        "    }\n"
        "}\n";

    QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/emojityper_act_p.js";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(script.toUtf8());
    f.close();

    QString pluginName = QString("et_act_p_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QDBusInterface kwinScripting("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", QDBusConnection::sessionBus());
    QDBusReply<int> reply = kwinScripting.call("loadScript", scriptPath, pluginName);
    if (reply.isValid() && reply.value() > 0) {
        int scriptId = reply.value();
        QDBusInterface scriptObj("org.kde.KWin", QString("/Scripting/Script%1").arg(scriptId), "org.kde.kwin.Script", QDBusConnection::sessionBus());
        scriptObj.call("run");
        kwinScripting.call("unloadScript", pluginName);
    }
    QFile::remove(scriptPath);
    return true;
}

bool TargetState::activateX11()
{
    if (m_targetWindow == 0) return false;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return false;

    XRaiseWindow(display, m_targetWindow);
    XSetInputFocus(display, m_targetWindow, RevertToParent, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
    return true;
}

void TargetState::clear()
{
    m_targetWindow = 0;
    m_targetUuid.clear();
    m_caretScreenPos = QPoint();
    m_valid = false;
}