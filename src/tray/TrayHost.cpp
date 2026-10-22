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
#include <QtCore/QDir>
#include <QtCore/QFile>
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

    // Setup tray FIRST so even if hotkey fails we have a working tray icon
    setupTray();
    setupConnections();

    bool hotkeyInstalled = false;
    if (m_useKdeHotkey && m_kdeHotkey) {
        hotkeyInstalled = m_kdeHotkey->install();
    } else if (m_x11Hotkey) {
        hotkeyInstalled = m_x11Hotkey->install();
    }

    if (!hotkeyInstalled) {
        // Non-fatal: app continues in tray-only mode. User can left-click the
        // tray icon or use "Show Emoji Panel" from the context menu.
        qWarning() << "Failed to install global hotkey (platform:" << platform << "desktop:" << desktop << ")";
        qWarning() << "EmojiTyper running in tray-only mode. Use tray icon to open emoji panel.";
        m_trayIcon->showMessage(
            "EmojiTyper",
            "Hotkey registration failed. Click the tray icon to open the emoji panel.",
            QSystemTrayIcon::Warning, 4000
        );
    }

    // Auto-setup XDG autostart on first run so EmojiTyper starts with the session
    setupAutostart();

    qDebug() << "TrayHost initialized successfully";
    return true;
}

void TrayHost::setupAutostart()
{
    // Find own executable path
    QString execPath = QCoreApplication::applicationFilePath();

    // XDG autostart directory (~/.config/autostart/)
    QString autostartDir = QDir::homePath() + "/.config/autostart";
    QDir().mkpath(autostartDir);

    QString desktopFilePath = autostartDir + "/org.emojiboard.EmojiTyper.desktop";

    // Only (re)write if not already present or if exec path changed
    QFile f(desktopFilePath);
    if (f.exists()) {
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(f.readAll());
            f.close();
            if (content.contains("Exec=" + execPath)) {
                qDebug() << "TrayHost: autostart entry already up-to-date";
                return;
            }
        }
    }

    QString desktopContent = QString(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=EmojiTyper\n"
        "GenericName=Emoji Picker\n"
        "Comment=Direct emoji typing tool for Linux (X11 & Wayland)\n"
        "Exec=%1\n"
        "Icon=emojityper\n"
        "Terminal=false\n"
        "Categories=Utility;Accessibility;Qt;\n"
        "Keywords=emoji;emoticon;picker;symbols;unicode;typing;\n"
        "StartupNotify=false\n"
        "X-GNOME-Autostart-enabled=true\n"
        "X-KDE-autostart-after=panel\n"
        "X-KDE-GlobalAccel-Target=true\n"
        "X-KDE-Shortcuts=Meta+.\n"
    ).arg(execPath);

    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(desktopContent.toUtf8());
        f.close();
        qDebug() << "TrayHost: wrote XDG autostart entry to" << desktopFilePath;
    } else {
        qWarning() << "TrayHost: failed to write autostart entry to" << desktopFilePath;
    }
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
    m_trayIcon->setToolTip("EmojiTyper — Press Meta+. for emoji panel");
    m_trayIcon->show();

    // Open panel on left-click (Trigger) of the tray icon
    connect(m_trayIcon.get(), &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger) {
                    showPanel();
                }
            });
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
void TrayHost::onAbout() { QMessageBox::about(nullptr, "EmojiTyper", "EmojiTyper v1.0.0\nPress Meta+. to open"); }
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
