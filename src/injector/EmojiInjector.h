/**
 * EmojiInjector - Injects emoji Unicode characters into target window
 * Linux/X11 implementation using XTest + wtype + clipboard fallback
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

class TargetState;

class EmojiInjector : public QObject
{
    Q_OBJECT
public:
    explicit EmojiInjector(QObject* parent = nullptr);
    ~EmojiInjector() override = default;

    // Insert emoji into target window (queued and batched for zero drops)
    bool insertEmoji(const QString& emoji, TargetState* targetState);

    // Immediately flush any queued emojis
    void flushQueue();

signals:
    void injectionFailed(const QString& reason);
    void clipboardFallbackUsed(const QString& emoji);

private slots:
    void processQueue();
    void restoreClipboard();

private:
    bool sendViaXTest(const QString& str);
    bool sendViaWtype(const QString& str);
    bool ensureYdotooldRunning();
    bool pasteViaYdotool();
    bool simulatePaste();
    bool saveClipboard();

    bool m_hasClipboardBackup = false;
    bool m_isWayland = false;
    bool m_wtypeTested = false;
    bool m_wtypeSupported = false;
    QString m_savedClipboardText;

    QString m_queuedEmojis;
    TargetState* m_pendingTargetState = nullptr;
    QTimer* m_batchTimer = nullptr;
    QTimer* m_restoreTimer = nullptr;
};