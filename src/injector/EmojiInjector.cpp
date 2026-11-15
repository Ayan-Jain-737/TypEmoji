/**
 * EmojiInjector implementation - Robust Linux/X11/Wayland Unicode injection
 * Uses an emoji queue to batch rapid typing and prevent dropped characters.
 */
#include "EmojiInjector.h"
#include "TargetState.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStandardPaths>
#include <QtCore/QThread>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

EmojiInjector::EmojiInjector(QObject *parent) : QObject(parent) {
    m_isWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");

    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    connect(m_batchTimer, &QTimer::timeout, this, &EmojiInjector::processQueue);

    m_restoreTimer = new QTimer(this);
    m_restoreTimer->setSingleShot(true);
    connect(m_restoreTimer, &QTimer::timeout, this, &EmojiInjector::restoreClipboard);

    ensureYdotooldRunning();
}

bool EmojiInjector::ensureYdotooldRunning() {
    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    QString socketPath = runtimeDir + "/.ydotool_socket";

    if (QFile::exists(socketPath)) {
        return true;
    }

    QProcess sysProc;
    sysProc.start("systemctl", QStringList() << "--user" << "start" << "ydotool.service");
    if (sysProc.waitForStarted(100) && sysProc.waitForFinished(300)) {
        if (QFile::exists(socketPath)) {
            return true;
        }
    }

    QProcess::startDetached("ydotoold", QStringList()
                                        << "--socket-path=" + socketPath
                                        << "--socket-perm=0600");
    for (int i = 0; i < 3; ++i) {
        if (QFile::exists(socketPath)) {
            return true;
        }
        QThread::msleep(20);
    }

    return QFile::exists(socketPath);
}

bool EmojiInjector::pasteViaYdotool() {
    if (!ensureYdotooldRunning()) {
        return false;
    }

    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    QString socketPath = runtimeDir + "/.ydotool_socket";

    QProcess ydoProc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("YDOTOOL_SOCKET", socketPath);
    ydoProc.setProcessEnvironment(env);

    // evdev: 29 = KEY_LEFTCTRL, 47 = KEY_V
    ydoProc.start("ydotool", QStringList() << "key" << "-d" << "2"
                                           << "29:1" << "47:1" << "47:0" << "29:0");
    if (!ydoProc.waitForStarted(150)) {
        return false;
    }
    ydoProc.waitForFinished(300);
    return (ydoProc.exitStatus() == QProcess::NormalExit && ydoProc.exitCode() == 0);
}

bool EmojiInjector::insertEmoji(const QString &emoji, TargetState *targetState) {
    if (emoji.isEmpty()) return false;

    // Direct X11 XTest path if available and not on Wayland
    if (!m_isWayland) {
        if (sendViaXTest(emoji)) {
            return true;
        }
    } else {
        // Direct wtype check once on Wayland
        if (!m_wtypeTested) {
            m_wtypeTested = true;
            m_wtypeSupported = sendViaWtype(emoji);
            if (m_wtypeSupported) {
                return true;
            }
        } else if (m_wtypeSupported) {
            if (sendViaWtype(emoji)) {
                return true;
            }
        }
    }

    // Queue emoji for batched clipboard paste to avoid dropping rapid clicks/typing
    m_queuedEmojis.append(emoji);
    m_pendingTargetState = targetState;

    if (!m_batchTimer->isActive()) {
        m_batchTimer->start(15);
    }

    return true;
}

void EmojiInjector::flushQueue() {
    if (m_batchTimer->isActive()) {
        m_batchTimer->stop();
        processQueue();
    }
}

void EmojiInjector::processQueue() {
    if (m_queuedEmojis.isEmpty()) return;

    QString batch = m_queuedEmojis;
    m_queuedEmojis.clear();
    TargetState* target = m_pendingTargetState;

    // 1. Preserve current clipboard content before overwriting
    saveClipboard();

    // 2. Put batch on clipboard (Qt in-memory + wl-copy on Wayland)
    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(batch, QClipboard::Clipboard);
        clipboard->setText(batch, QClipboard::Selection);
    }
    if (m_isWayland) {
        QProcess::startDetached("wl-copy", QStringList() << batch);
    }

    // 3. Ensure target window has focus
    if (target && target->isValid()) {
        target->ensureForeground();
        QCoreApplication::processEvents();
    }

    // 4. Simulate paste
    simulatePaste();

    // 5. Restore user clipboard after batch typing finishes
    m_restoreTimer->start(400);

    emit clipboardFallbackUsed(batch);
}

bool EmojiInjector::simulatePaste() {
    // 1. ydotool (evdev hardware level, works on KDE Wayland, GNOME, Sway, X11)
    if (pasteViaYdotool()) {
        return true;
    }

    // 2. wtype on Wayland
    if (m_isWayland) {
        QProcess proc;
        proc.start("wtype", QStringList() << "-M" << "ctrl" << "-k" << "v" << "-m" << "ctrl");
        if (proc.waitForStarted(100) && proc.waitForFinished(300)) {
            if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                return true;
            }
        }
    }

    // 3. xdotool (X11 / XWayland)
    {
        QProcess proc;
        proc.start("xdotool", QStringList() << "key" << "ctrl+v");
        if (proc.waitForStarted(100) && proc.waitForFinished(300)) {
            if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                return true;
            }
        }
    }

    // 4. Native X11 XTest key event
    if (!m_isWayland) {
        Display *display = XOpenDisplay(nullptr);
        if (display) {
            KeyCode ctrlKey = XKeysymToKeycode(display, XK_Control_L);
            KeyCode vKey = XKeysymToKeycode(display, XK_v);
            if (ctrlKey && vKey) {
                XTestFakeKeyEvent(display, ctrlKey, True, CurrentTime);
                XTestFakeKeyEvent(display, vKey, True, CurrentTime);
                XTestFakeKeyEvent(display, vKey, False, CurrentTime);
                XTestFakeKeyEvent(display, ctrlKey, False, CurrentTime);
                XFlush(display);
                XCloseDisplay(display);
                return true;
            }
            XCloseDisplay(display);
        }
    }

    return false;
}

bool EmojiInjector::sendViaWtype(const QString &str) {
    QProcess process;
    process.start("wtype", QStringList() << str);
    if (!process.waitForStarted(80)) {
        return false;
    }
    process.waitForFinished(150);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool EmojiInjector::sendViaXTest(const QString &str) {
    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        return false;
    }

    int event_base, error_base, major_version, minor_version;
    if (!XTestQueryExtension(display, &event_base, &error_base, &major_version, &minor_version)) {
        XCloseDisplay(display);
        return false;
    }

    bool success = true;
    auto ucs4 = str.toUcs4();
    for (uint32_t cp : ucs4) {
        KeySym keysym = 0x01000000 | cp;
        KeyCode keycode = XKeysymToKeycode(display, keysym);
        if (keycode == 0) {
            success = false;
            break;
        }

        XTestFakeKeyEvent(display, keycode, True, CurrentTime);
        XTestFakeKeyEvent(display, keycode, False, CurrentTime);
    }

    XFlush(display);
    XCloseDisplay(display);
    return success;
}

bool EmojiInjector::saveClipboard() {
    if (m_hasClipboardBackup) {
        return true;
    }

    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        m_savedClipboardText = clipboard->text(QClipboard::Clipboard);
        if (!m_savedClipboardText.isEmpty()) {
            m_hasClipboardBackup = true;
            return true;
        }
    }

    if (m_isWayland) {
        QProcess proc;
        proc.start("wl-paste", QStringList() << "--no-newline");
        if (proc.waitForStarted(50) && proc.waitForFinished(100)) {
            m_savedClipboardText = QString::fromUtf8(proc.readAllStandardOutput());
            m_hasClipboardBackup = !m_savedClipboardText.isEmpty();
            return m_hasClipboardBackup;
        }
    }

    return false;
}

void EmojiInjector::restoreClipboard() {
    if (!m_hasClipboardBackup || m_savedClipboardText.isEmpty()) {
        m_hasClipboardBackup = false;
        m_savedClipboardText.clear();
        return;
    }

    if (m_isWayland) {
        QProcess proc;
        proc.start("wl-copy", QStringList() << m_savedClipboardText);
        if (proc.waitForStarted(100)) {
            proc.waitForFinished(300);
        }
    }

    QClipboard *clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_savedClipboardText, QClipboard::Clipboard);
    }

    m_hasClipboardBackup = false;
    m_savedClipboardText.clear();
}
