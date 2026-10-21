/**
 * EmojiInjector implementation - Linux/X11/Wayland direct Unicode injection
 *
 * Injection strategy:
 *   1. Direct wtype (Wayland only, requires compositor text-input-v3 support)
 *   2. Universal Clipboard-paste:
 *      - Step A: Copy emoji to system clipboard (wl-copy + QClipboard)
 *      - Step B: Wait for compositor focus restoration
 *      - Step C: Simulate Ctrl+V:
 *          1) ydotool evdev hardware key event (works on ALL Wayland compositors & X11)
 *          2) wtype -M ctrl -k v -m ctrl (wlroots Wayland)
 *          3) xdotool key ctrl+v (X11 / XWayland)
 *          4) XTest fake key event (native X11)
 *      - Step D: Restore original user clipboard content after brief delay
 */
#include "EmojiInjector.h"
#include "TargetState.h"
#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QStandardPaths>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QCoreApplication>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

EmojiInjector::EmojiInjector(QObject* parent) : QObject(parent)
{
    m_isWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
    // Pre-emptively ensure ydotool daemon is ready
    ensureYdotooldRunning();
}

bool EmojiInjector::ensureYdotooldRunning()
{
    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    QString socketPath = runtimeDir + "/.ydotool_socket";

    if (QFile::exists(socketPath)) {
        return true;
    }

    // Try starting ydotool user service via systemctl
    QProcess sysProc;
    sysProc.start("systemctl", QStringList() << "--user" << "start" << "ydotool.service");
    if (sysProc.waitForStarted(200) && sysProc.waitForFinished(1000)) {
        for (int i = 0; i < 5; ++i) {
            if (QFile::exists(socketPath)) {
                return true;
            }
            QThread::msleep(50);
        }
    }

    // Direct fallback: spawn ydotoold daemon
    QProcess::startDetached("ydotoold", QStringList() << "--socket-path=" + socketPath << "--socket-perm=0600");
    for (int i = 0; i < 5; ++i) {
        if (QFile::exists(socketPath)) {
            return true;
        }
        QThread::msleep(50);
    }

    return QFile::exists(socketPath);
}

bool EmojiInjector::pasteViaYdotool()
{
    if (!ensureYdotooldRunning()) {
        return false;
    }

    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    }
    QString socketPath = runtimeDir + "/.ydotool_socket";

    // evdev key codes:
    // 29 = KEY_LEFTCTRL
    // 47 = KEY_V
    // Sequence: 29:1 (Ctrl down), 47:1 (V down), 47:0 (V up), 29:0 (Ctrl up)
    QProcess ydoProc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("YDOTOOL_SOCKET", socketPath);
    ydoProc.setProcessEnvironment(env);

    ydoProc.start("ydotool", QStringList() << "key" << "29:1" << "47:1" << "47:0" << "29:0");
    if (!ydoProc.waitForStarted(300)) {
        return false;
    }
    ydoProc.waitForFinished(1000);
    return (ydoProc.exitStatus() == QProcess::NormalExit && ydoProc.exitCode() == 0);
}

bool EmojiInjector::insertEmoji(const QString& emoji, TargetState* targetState)
{
    if (emoji.isEmpty()) return false;

    // Direct wtype: only works on compositors with zwp_virtual_keyboard_v1 (e.g. Sway, Hyprland)
    // On compositors without it (like KWin), sendViaWtype fails immediately and falls through.
    if (m_isWayland) {
        if (sendViaWtype(emoji)) {
            qDebug() << "EmojiInjector: direct typing via wtype succeeded";
            return true;
        }
    }

    // Universal clipboard paste pipeline (works reliably across all Linux compositors)
    return clipboardFallback(emoji, targetState);
}

bool EmojiInjector::sendViaYdotool(const QString& str)
{
    // Note: ydotool type only works with standard keymapped ASCII characters.
    // For non-ASCII Unicode emojis, clipboardFallback with pasteViaYdotool is used instead.
    QProcess process;
    process.start("ydotool", QStringList() << "type" << "--key-delay" << "0" << str);
    if (!process.waitForStarted(200)) {
        return false;
    }
    process.waitForFinished(1000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool EmojiInjector::sendViaWtype(const QString& str)
{
    QProcess process;
    process.start("wtype", QStringList() << str);
    if (!process.waitForStarted(200)) {
        return false;
    }
    process.waitForFinished(1000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

bool EmojiInjector::sendViaXTest(const QString& str)
{
    Display* display = XOpenDisplay(nullptr);
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

bool EmojiInjector::clipboardFallback(const QString& emoji, TargetState* targetState)
{
    // 1. Save existing clipboard content for later restoration
    saveClipboard();

    // 2. Set emoji to system clipboard
    if (m_isWayland) {
        QProcess copyProc;
        copyProc.start("wl-copy", QStringList() << emoji);
        if (copyProc.waitForStarted(200)) {
            copyProc.waitForFinished(500);
        }
    }

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(emoji, QClipboard::Clipboard);
        clipboard->setText(emoji, QClipboard::Selection);
    }

    // Small delay to ensure clipboard buffers have propagated
    QThread::msleep(40);

    // 3. For X11, ensure target window has focus
    if (!m_isWayland && targetState && targetState->isValid()) {
        targetState->ensureForeground();
        QCoreApplication::processEvents();
        QThread::msleep(40);
    }

    // 4. Simulate paste (Ctrl+V) using priority backends
    bool pasteSimulated = false;

    // Backend 1: ydotool key (evdev hardware level, works on KDE Wayland, GNOME, Sway, X11)
    if (pasteViaYdotool()) {
        pasteSimulated = true;
        qDebug() << "EmojiInjector: pasted via ydotool key";
    }

    // Backend 2: wtype (Wayland virtual keyboard)
    if (!pasteSimulated && m_isWayland) {
        QProcess pasteProc;
        pasteProc.start("wtype", QStringList() << "-M" << "ctrl" << "-k" << "v" << "-m" << "ctrl");
        if (pasteProc.waitForStarted(200)) {
            pasteProc.waitForFinished(1000);
            pasteSimulated = (pasteProc.exitStatus() == QProcess::NormalExit && pasteProc.exitCode() == 0);
            if (pasteSimulated) qDebug() << "EmojiInjector: pasted via wtype";
        }
    }

    // Backend 3: xdotool (X11 / XWayland)
    if (!pasteSimulated) {
        QProcess xdoProc;
        xdoProc.start("xdotool", QStringList() << "key" << "ctrl+v");
        if (xdoProc.waitForStarted(200)) {
            xdoProc.waitForFinished(1000);
            pasteSimulated = (xdoProc.exitStatus() == QProcess::NormalExit && xdoProc.exitCode() == 0);
            if (pasteSimulated) qDebug() << "EmojiInjector: pasted via xdotool";
        }
    }

    // Backend 4: XTest fake key event (native X11)
    if (!pasteSimulated && !m_isWayland) {
        Display* display = XOpenDisplay(nullptr);
        if (display) {
            KeyCode ctrlKey = XKeysymToKeycode(display, XK_Control_L);
            KeyCode vKey = XKeysymToKeycode(display, XK_v);

            if (ctrlKey && vKey) {
                XTestFakeKeyEvent(display, ctrlKey, True, CurrentTime);
                XTestFakeKeyEvent(display, vKey, True, CurrentTime);
                XTestFakeKeyEvent(display, vKey, False, CurrentTime);
                XTestFakeKeyEvent(display, ctrlKey, False, CurrentTime);
                XFlush(display);
                pasteSimulated = true;
                qDebug() << "EmojiInjector: pasted via XTest";
            }
            XCloseDisplay(display);
        }
    }

    // 5. Restore user's previous clipboard after inactivity
    if (m_hasClipboardBackup) {
        if (!m_restoreTimer) {
            m_restoreTimer = new QTimer(this);
            m_restoreTimer->setSingleShot(true);
            connect(m_restoreTimer, &QTimer::timeout, this, &EmojiInjector::restoreClipboard);
        }
        m_restoreTimer->start(1200);
    }

    if (!pasteSimulated) {
        qWarning() << "EmojiInjector: all paste simulation methods failed";
    }

    emit clipboardFallbackUsed(emoji);
    return pasteSimulated;
}

bool EmojiInjector::saveClipboard()
{
    if (m_hasClipboardBackup) {
        // Keep the original backup from the beginning of the typing session
        return true;
    }

    if (m_isWayland) {
        QProcess proc;
        proc.start("wl-paste", QStringList() << "--no-newline");
        if (proc.waitForStarted(200) && proc.waitForFinished(1000)) {
            m_savedClipboardText = QString::fromUtf8(proc.readAllStandardOutput());
            m_hasClipboardBackup = !m_savedClipboardText.isEmpty();
            return m_hasClipboardBackup;
        }
        m_hasClipboardBackup = false;
        return false;
    } else {
        QClipboard* clipboard = QApplication::clipboard();
        if (clipboard) {
            m_savedClipboardText = clipboard->text(QClipboard::Clipboard);
            m_hasClipboardBackup = !m_savedClipboardText.isEmpty();
            return m_hasClipboardBackup;
        }
        return false;
    }
}

void EmojiInjector::restoreClipboard()
{
    if (!m_hasClipboardBackup || m_savedClipboardText.isEmpty()) return;

    if (m_isWayland) {
        QProcess proc;
        proc.start("wl-copy", QStringList() << m_savedClipboardText);
        if (proc.waitForStarted(200)) {
            proc.waitForFinished(1000);
        }
    }

    QClipboard* clipboard = QApplication::clipboard();
    if (clipboard) {
        clipboard->setText(m_savedClipboardText, QClipboard::Clipboard);
    }

    m_hasClipboardBackup = false;
    m_savedClipboardText.clear();
}
