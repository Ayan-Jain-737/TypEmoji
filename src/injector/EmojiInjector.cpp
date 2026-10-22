/**
 * EmojiInjector implementation - Linux/X11/Wayland direct Unicode injection
 *
 * Injection strategy:
 *   1. Direct wtype (Wayland only, requires compositor text-input-v3 support)
 *   2. Universal Clipboard-paste:
 *      - Step A: Copy emoji to system clipboard (wl-copy + QClipboard)
 *      - Step B: Wait for compositor focus restoration
 *      - Step C: Simulate Ctrl+V:
 *          1) ydotool evdev hardware key event (works on ALL Wayland
 * compositors & X11) 2) wtype -M ctrl -k v -m ctrl (wlroots Wayland) 3) xdotool
 * key ctrl+v (X11 / XWayland) 4) XTest fake key event (native X11)
 *      - Step D: Restore original user clipboard content after brief delay
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
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

EmojiInjector::EmojiInjector(QObject *parent) : QObject(parent) {
  m_isWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
  // Pre-emptively ensure ydotool daemon is ready
  ensureYdotooldRunning();
}

bool EmojiInjector::ensureYdotooldRunning() {
  QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (runtimeDir.isEmpty()) {
    runtimeDir =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  }
  QString socketPath = runtimeDir + "/.ydotool_socket";

  if (QFile::exists(socketPath)) {
    return true;
  }

  // Try starting ydotool user service via systemctl
  QProcess sysProc;
  sysProc.start("systemctl", QStringList()
                                 << "--user" << "start" << "ydotool.service");
  if (sysProc.waitForStarted(200) && sysProc.waitForFinished(1000)) {
    for (int i = 0; i < 5; ++i) {
      if (QFile::exists(socketPath)) {
        return true;
      }
      QThread::msleep(50);
    }
  }

  // Direct fallback: spawn ydotoold daemon
  QProcess::startDetached("ydotoold", QStringList()
                                          << "--socket-path=" + socketPath
                                          << "--socket-perm=0600");
  for (int i = 0; i < 5; ++i) {
    if (QFile::exists(socketPath)) {
      return true;
    }
    QThread::msleep(50);
  }

  return QFile::exists(socketPath);
}

bool EmojiInjector::pasteViaYdotool() {
  if (!ensureYdotooldRunning()) {
    return false;
  }

  QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
  if (runtimeDir.isEmpty()) {
    runtimeDir =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
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

  ydoProc.start("ydotool", QStringList() << "key" << "-d" << "2" << "29:1"
                                         << "47:1" << "47:0" << "29:0");
  if (!ydoProc.waitForStarted(200)) {
    return false;
  }
  ydoProc.waitForFinished(500);
  return (ydoProc.exitStatus() == QProcess::NormalExit &&
          ydoProc.exitCode() == 0);
}

bool EmojiInjector::insertEmoji(const QString &emoji,
                                TargetState *targetState) {
  if (emoji.isEmpty())
    return false;

  // Direct wtype: only works on compositors with zwp_virtual_keyboard_v1 (e.g.
  // Sway, Hyprland). Test once on Wayland: if not supported (e.g. KWin), cache
  // the result to avoid running a doomed subprocess every click!
  if (m_isWayland) {
    if (!m_wtypeTested) {
      m_wtypeTested = true;
      m_wtypeSupported = sendViaWtype(emoji);
      if (m_wtypeSupported) {
        qDebug() << "EmojiInjector: direct typing via wtype succeeded";
        return true;
      }
    } else if (m_wtypeSupported) {
      if (sendViaWtype(emoji)) {
        return true;
      }
    }
  }

  // Universal high-performance clipboard paste pipeline (works reliably across
  // all Linux compositors)
  return clipboardFallback(emoji, targetState);
}

bool EmojiInjector::sendViaYdotool(const QString &str) {
  // Note: ydotool type only works with standard keymapped ASCII characters.
  // For non-ASCII Unicode emojis, clipboardFallback with pasteViaYdotool is
  // used instead.
  QProcess process;
  process.start("ydotool", QStringList()
                               << "type" << "--key-delay" << "0" << str);
  if (!process.waitForStarted(200)) {
    return false;
  }
  process.waitForFinished(1000);
  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
}

bool EmojiInjector::sendViaWtype(const QString &str) {
  QProcess process;
  process.start("wtype", QStringList() << str);
  if (!process.waitForStarted(100)) {
    return false;
  }
  process.waitForFinished(200);
  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
}

bool EmojiInjector::sendViaXTest(const QString &str) {
  Display *display = XOpenDisplay(nullptr);
  if (!display) {
    return false;
  }

  int event_base, error_base, major_version, minor_version;
  if (!XTestQueryExtension(display, &event_base, &error_base, &major_version,
                           &minor_version)) {
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

bool EmojiInjector::clipboardFallback(const QString &emoji,
                                      TargetState *targetState) {
  // 1. Set emoji to system clipboard via Qt's native clipboard interface
  // (in-memory 0ms) No subprocesses, no wl-copy races or crashes!
  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard) {
    clipboard->setText(emoji, QClipboard::Clipboard);
    clipboard->setText(emoji, QClipboard::Selection);
  }
  if (m_isWayland) {
    QProcess::startDetached("wl-copy", QStringList() << emoji);
  }

  // 2. Ensure target window has focus before simulating paste (Wayland KWin &
  // X11)
  if (targetState && targetState->isValid()) {
    targetState->ensureForeground();
    QCoreApplication::processEvents();
    QThread::msleep(25);
  }

  // 3. Simulate paste (Ctrl+V) using priority backends
  bool pasteSimulated = false;

  // Backend 1: ydotool key (evdev hardware level, works in ~1ms on KDE Wayland,
  // GNOME, Sway, X11)
  if (pasteViaYdotool()) {
    pasteSimulated = true;
    qDebug() << "EmojiInjector: pasted via ydotool key";
  }

  // Backend 2: wtype (Wayland virtual keyboard)
  if (!pasteSimulated && m_isWayland) {
    QProcess pasteProc;
    pasteProc.start("wtype", QStringList() << "-M" << "ctrl" << "-k" << "v"
                                           << "-m" << "ctrl");
    if (pasteProc.waitForStarted(200)) {
      pasteProc.waitForFinished(1000);
      pasteSimulated = (pasteProc.exitStatus() == QProcess::NormalExit &&
                        pasteProc.exitCode() == 0);
      if (pasteSimulated)
        qDebug() << "EmojiInjector: pasted via wtype";
    }
  }

  // Backend 3: xdotool (X11 / XWayland)
  if (!pasteSimulated) {
    QProcess xdoProc;
    xdoProc.start("xdotool", QStringList() << "key" << "ctrl+v");
    if (xdoProc.waitForStarted(200)) {
      xdoProc.waitForFinished(1000);
      pasteSimulated = (xdoProc.exitStatus() == QProcess::NormalExit &&
                        xdoProc.exitCode() == 0);
      if (pasteSimulated)
        qDebug() << "EmojiInjector: pasted via xdotool";
    }
  }

  // Backend 4: XTest fake key event (native X11)
  if (!pasteSimulated && !m_isWayland) {
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
        pasteSimulated = true;
        qDebug() << "EmojiInjector: pasted via XTest";
      }
      XCloseDisplay(display);
    }
  }

  // Brief delay to allow target application to process Ctrl+V paste event
  QThread::msleep(15);

  if (!pasteSimulated) {
    qWarning() << "EmojiInjector: all paste simulation methods failed";
  }

  emit clipboardFallbackUsed(emoji);
  return pasteSimulated;
}

bool EmojiInjector::saveClipboard() {
  if (m_hasClipboardBackup) {
    // Keep the original backup from the beginning of the typing session
    return true;
  }

  // In Qt 6, QClipboard maintains the clipboard buffer in memory across Wayland
  // & X11 (0ms)
  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard) {
    m_savedClipboardText = clipboard->text(QClipboard::Clipboard);
    if (!m_savedClipboardText.isEmpty()) {
      m_hasClipboardBackup = true;
      return true;
    }
  }

  // Fallback to wl-paste only if QClipboard was empty and running on Wayland
  if (m_isWayland) {
    QProcess proc;
    proc.start("wl-paste", QStringList() << "--no-newline");
    if (proc.waitForStarted(50) && proc.waitForFinished(100)) {
      m_savedClipboardText = QString::fromUtf8(proc.readAllStandardOutput());
      m_hasClipboardBackup = !m_savedClipboardText.isEmpty();
      return m_hasClipboardBackup;
    }
  }

  m_hasClipboardBackup = false;
  return false;
}

void EmojiInjector::restoreClipboard() {
  if (!m_hasClipboardBackup || m_savedClipboardText.isEmpty())
    return;

  if (m_isWayland) {
    QProcess proc;
    proc.start("wl-copy", QStringList() << m_savedClipboardText);
    if (proc.waitForStarted(200)) {
      proc.waitForFinished(1000);
    }
  }

  QClipboard *clipboard = QApplication::clipboard();
  if (clipboard) {
    clipboard->setText(m_savedClipboardText, QClipboard::Clipboard);
  }

  m_hasClipboardBackup = false;
  m_savedClipboardText.clear();
}
