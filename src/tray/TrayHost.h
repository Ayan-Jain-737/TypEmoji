/**
 * TrayHost - System tray icon and single-instance host for EmojiTyper
 */
#pragma once

#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <memory>

class EmojiDatabase;
class EmojiInjector;
class TargetState;
class GlobalHotkey;
class KGlobalAccelHotkey;
class EmojiPanel;

class TrayHost : public QObject
{
    Q_OBJECT
public:
    explicit TrayHost(QObject* parent = nullptr);
    ~TrayHost() override;

    bool initialize();
    void showPanel(const QPoint& pos = QPoint(-1, -1));
    void showSettings();
    void quit();

signals:
    void panelClosed();

private slots:
    void onHotkeyPressed(const QPoint& caretPos);
    void onPanelClosed();
    void onAbout();
    void onQuit();

private:
    void setupTray();
    void setupConnections();
    void createPanel();

    std::unique_ptr<QSystemTrayIcon> m_trayIcon;
    std::unique_ptr<QMenu> m_trayMenu;

    std::unique_ptr<EmojiDatabase> m_database;
    std::unique_ptr<EmojiInjector> m_injector;
    std::unique_ptr<TargetState> m_targetState;
    std::unique_ptr<GlobalHotkey> m_x11Hotkey;
    std::unique_ptr<KGlobalAccelHotkey> m_kdeHotkey;
    std::unique_ptr<EmojiPanel> m_panel;
    bool m_useKdeHotkey = false;
};
