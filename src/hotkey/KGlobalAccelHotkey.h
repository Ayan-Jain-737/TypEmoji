/**
 * KGlobalAccelHotkey - KDE KGlobalAccel global hotkey implementation for Wayland
 * Uses KGlobalAccel to register global shortcuts that work on Wayland
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtGui/QAction>
#include <memory>
#include <KActionCollection>

class KGlobalAccelHotkey : public QObject
{
    Q_OBJECT
public:
    explicit KGlobalAccelHotkey(QObject* parent = nullptr);
    ~KGlobalAccelHotkey() override;

    bool install();
    void uninstall();

    void setHotkey(int modifiers, int keyCode);
    int modifiers() const { return m_modifiers; }
    int keyCode() const { return m_keyCode; }

    bool isInstalled() const { return m_installed; }

signals:
    void hotkeyPressed(const QPoint& caretPos);
    void hotkeyReleased();

public slots:
    void onGlobalShortcutPressed(const QString& component, const QString& shortcut, qlonglong timestamp);

private:
    void setupAction();
    void triggerHotkey();

    std::unique_ptr<KActionCollection> m_actionCollection;
    std::unique_ptr<QAction> m_action;
    int m_modifiers = 0;
    int m_keyCode = 0x2e;  // Qt::Key_Period
    bool m_installed = false;
    qint64 m_lastHotkeyTime = 0;
};