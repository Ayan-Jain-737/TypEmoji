/**
 * TargetState - Captures and holds the target window/control state for Linux
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>

class TargetState : public QObject
{
    Q_OBJECT
public:
    explicit TargetState(QObject* parent = nullptr);
    ~TargetState() override = default;

    // Capture current foreground window and caret position
    bool capture();

    // Check if captured state is valid
    bool isValid() const;

    // Getters
    unsigned long targetWindow() const { return m_targetWindow; }
    QPoint caretScreenPos() const { return m_caretScreenPos; }

    // Check if target requires special fallback handling
    bool isTargetElevated() const { return false; } // Linux doesn't have UIPI restrictions

    // Ensure target has focus before injection
    bool ensureForeground();

    // Clear captured state
    void clear();

private:
    unsigned long m_targetWindow = 0;
    QPoint m_caretScreenPos;
    bool m_valid = false;
};