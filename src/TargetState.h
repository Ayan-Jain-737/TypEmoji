/**
 * TargetState - Captures and holds the target window/control state for Linux
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QString>

class TargetState : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.emojiboard.TargetState")
public:
    explicit TargetState(QObject* parent = nullptr);
    ~TargetState() override { clear(); }

    // Capture current foreground window and caret position
    bool capture();

    // Check if captured state is valid
    bool isValid() const;

    // Getters
    unsigned long targetWindow() const { return m_targetWindow; }
    QString targetUuid() const { return m_targetUuid; }
    QPoint caretScreenPos() const { return m_caretScreenPos; }

    // Check if target requires special fallback handling
    bool isTargetElevated() const { return false; }

    // Ensure target has focus before injection
    bool ensureForeground();

    // Ensure EmojiPanel stays on top (Wayland keepAbove)
    bool ensurePanelAbove();

    // Re-activate EmojiPanel after injection
    bool activateEmojiPanel();

    // Clear captured state
    void clear();

public slots:
    void setTargetUuid(const QString& uuid);

private:
    bool captureKWinWayland();
    bool captureX11();
    bool activateKWinWayland();
    bool activateX11();

    unsigned long m_targetWindow = 0;
    QString m_targetUuid;
    QPoint m_caretScreenPos;
    bool m_valid = false;
    bool m_isWayland = false;
};