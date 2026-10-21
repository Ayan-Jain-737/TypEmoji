/**
 * TrayHost implementation - System tray and application orchestration
 */
#include "TrayHost.h"
#include "../data/EmojiDatabase.h"
#include "../injector/EmojiInjector.h"
#include "../TargetState.h"
#include "../hotkey/GlobalHotkey.h"
#include "../hotkey/KGlobalAccelHotkey.h"
#include "../panel/EmojiPanel.h"
#include "../settings/Settings.h"
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtGui/QAction>
#include <QtCore/QDebug>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtCore/QCoreApplication>
#include <QtGui/QGuiApplication>
#include <iostream>

TrayHost::TrayHost(QObject* parent) : QObject(parent) {}
TrayHost::~TrayHost() {}

bool TrayHost::initialize()
{
    qDebug() << "TrayHost: initializing";
    m_database = std::make_unique<EmojiDatabase>();
    if (!m_database->load()) {
        qCritical() << "Failed to load emoji database";
        return false;
    }
    qDebug() << "TrayHost: database loaded";
    m_injector = std::make_unique<EmojiInjector>();
    m_targetState = std::make_unique<TargetState>();

    // Determine which hotkey backend to use
    // Prefer KGlobalAccel on KDE (Wayland or X11), fall back to X11 XGrabKey otherwise
    QString platform = QGuiApplication::platformName().toLower();
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    bool isKde = desktop.contains("kde") || desktop.contains("plasma");
    m_useKdeHotkey = isKde;  // Only use KGlobalAccel on KDE desktops
    qDebug() << "TrayHost: platform=" << platform << "desktop=" << desktop << "useKDE=" << m_useKdeHotkey;

    if (m_useKdeHotkey) {
        m_kdeHotkey = std::make_unique<KGlobalAccelHotkey>();
        Settings& settings = Settings::instance();
        m_kdeHotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());
    } else {
        m_x11Hotkey = std::make_unique<GlobalHotkey>();
        Settings& settings = Settings::instance();
        m_x11Hotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());
    }

    setupTray();
    setupConnections();
    
    bool hotkeyInstalled = false;
    if (m_useKdeHotkey && m_kdeHotkey) {
        hotkeyInstalled = m_kdeHotkey->install();
    } else if (m_x11Hotkey) {
        hotkeyInstalled = m_x11Hotkey->install();
    }
    
    if (!hotkeyInstalled) {
        qWarning() << "Failed to install hotkey (platform:" << platform << "desktop:" << desktop << ")";
        return false;
    }
    qDebug() << "TrayHost initialized successfully";
    return true;
}

void TrayHost::setupTray()
{
    m_trayMenu = std::make_unique<QMenu>();
    QAction* showAction = m_trayMenu->addAction("Show Emoji Panel");
    connect(showAction, &QAction::triggered, this, [this](){ showPanel(); });
    m_trayMenu->addSeparator();
    QAction* quitAction = m_trayMenu->addAction("Exit");
    connect(quitAction, &QAction::triggered, this, &TrayHost::onQuit);
    m_trayIcon = std::make_unique<QSystemTrayIcon>();
    QIcon icon = QIcon::fromTheme("emojityper", QIcon("assets/icons/emojityper.svg"));
    if (icon.isNull()) {
        icon = QIcon::fromTheme("face-smile", QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    }
    m_trayIcon->setIcon(icon);
    m_trayIcon->setContextMenu(m_trayMenu.get());
    m_trayIcon->setToolTip("EmojiTyper");
    m_trayIcon->show();
}

void TrayHost::setupConnections()
{
    if (m_useKdeHotkey && m_kdeHotkey) {
        connect(m_kdeHotkey.get(), &KGlobalAccelHotkey::hotkeyPressed, this, &TrayHost::onHotkeyPressed);
    } else if (m_x11Hotkey) {
        connect(m_x11Hotkey.get(), &GlobalHotkey::hotkeyPressed, this, &TrayHost::onHotkeyPressed);
    }
}

void TrayHost::createPanel()
{
    if (!m_panel) {
        m_panel = std::make_unique<EmojiPanel>(m_database.get(), m_injector.get(), m_targetState.get());
        connect(m_panel.get(), &EmojiPanel::panelClosed, this, &TrayHost::onPanelClosed);
    }
}

void TrayHost::showPanel(const QPoint& pos)
{
    qDebug() << "TrayHost::showPanel called with pos:" << pos;
    createPanel();
    if (m_panel) {
        if (m_panel->isVisible()) {
            m_panel->hidePanel();
            return;
        }
        if (m_targetState && m_targetState->capture())
            m_panel->showAt(pos);
        else
            m_panel->showAt();
    }
}

void TrayHost::onHotkeyPressed(const QPoint& caretPos)
{
    qDebug() << "TrayHost: hotkey pressed, toggling panel";
    // Toggle: if panel is visible, hide it; otherwise show it
    if (m_panel && m_panel->isVisible()) {
        m_panel->hidePanel();
    } else {
        showPanel(caretPos);
    }
}

void TrayHost::onPanelClosed() { emit panelClosed(); }
void TrayHost::showSettings() { QMessageBox::information(nullptr, "Settings", "Settings UI coming soon"); }
void TrayHost::onAbout() { QMessageBox::about(nullptr, "EmojiTyper", "EmojiTyper v1.0.0\nPress Win+. to open"); }
void TrayHost::onQuit() { quit(); }
void TrayHost::quit()
{
    if (m_useKdeHotkey && m_kdeHotkey) {
        m_kdeHotkey->uninstall();
    } else if (m_x11Hotkey) {
        m_x11Hotkey->uninstall();
    }
    m_trayIcon->hide();
    QCoreApplication::quit();
}
