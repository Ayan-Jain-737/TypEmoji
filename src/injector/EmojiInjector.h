/**
 * EmojiInjector - Injects emoji Unicode characters into target window
 * Linux/X11 implementation using XTest + wtype + clipboard fallback
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class TargetState;

class EmojiInjector : public QObject
{
    Q_OBJECT
public:
    explicit EmojiInjector(QObject* parent = nullptr);
    ~EmojiInjector() override = default;

    // Insert emoji string into the target window
    // Returns true on success, false if clipboard fallback needed
    bool insertEmoji(const QString& emoji, TargetState* targetState);

    // Fallback: copy to clipboard and simulate Ctrl+V
    bool clipboardFallback(const QString& emoji, TargetState* targetState);

signals:
    void injectionFailed(const QString& reason);
    void clipboardFallbackUsed(const QString& emoji);

private:
    // Send Unicode string via ydotool (Wayland native, no virtual keyboard needed)
    bool sendViaYdotool(const QString& str);

    // Send Unicode string via XTest (X11 path through Xwayland)
    bool sendViaXTest(const QString& str);

    // Send Unicode string via wtype (Wayland native)
    bool sendViaWtype(const QString& str);

    // Send Unicode string via XSendEvent (WM_UTF8_STRING)
    bool sendViaXSendEvent(const QString& str);

    // Find keycode for a keysym
    int keycodeForKeysym(unsigned long keysym) const;

    // Ensure ydotoold daemon is running and socket is ready
    bool ensureYdotooldRunning();

    // Paste via ydotool hardware-level virtual key events (evdev Ctrl+V)
    bool pasteViaYdotool();

    // Restore original clipboard content
    void restoreClipboard();

    // Save current clipboard content
    bool saveClipboard();

    bool m_hasClipboardBackup = false;
    bool m_isWayland = false;
    QString m_savedClipboardText;
    class QTimer* m_restoreTimer = nullptr;
};