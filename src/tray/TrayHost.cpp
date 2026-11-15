/**
 * TrayHost implementation - System tray and application orchestration
 */
#include "TrayHost.h"
#include "../data/EmojiDatabase.h"
#include "../injector/EmojiInjector.h"
#include "../TargetState.h"
#include "../hotkey/GlobalHotkey.h"
#include "../hotkey/KGlobalAccelHotkey.h"
#include "../hotkey/PortalHotkey.h"
#include "../panel/EmojiPanel.h"
#include "../settings/Settings.h"
#include "../settings/SettingsDialog.h"
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtGui/QAction>
#include <QtCore/QDebug>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <unistd.h>

TrayHost::TrayHost(QObject* parent) : QObject(parent)
{
    QString appPath = QCoreApplication::applicationFilePath();
    QFileInfo fi(appPath);
    if (fi.exists()) {
        m_binaryModTimeMs = fi.lastModified().toMSecsSinceEpoch();
    }

    m_reloadDebounceTimer = new QTimer(this);
    m_reloadDebounceTimer->setSingleShot(true);
    m_reloadDebounceTimer->setInterval(400);
    connect(m_reloadDebounceTimer, &QTimer::timeout, this, [this]() {
        reexecSelf();
    });

    m_fileWatcher = std::make_unique<QFileSystemWatcher>(this);
    if (QFile::exists(appPath)) {
        m_fileWatcher->addPath(appPath);
        connect(m_fileWatcher.get(), &QFileSystemWatcher::fileChanged, this, [this, appPath](const QString&) {
            qDebug() << "TrayHost: Executable modified on disk, scheduling re-exec...";
            if (QFile::exists(appPath)) {
                m_fileWatcher->addPath(appPath);
            }
            m_reloadDebounceTimer->start();
        });
    }
}
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
    // 1. Try KGlobalAccel on KDE (Wayland or X11)
    // 2. Try XDG Desktop Portal on Wayland (GNOME 44+, Sway, Hyprland)
    // 3. Fall back to X11 XGrabKey (X11 desktops, XFCE, i3, etc.)
    QString platform = QGuiApplication::platformName().toLower();
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    bool isKde = desktop.contains("kde") || desktop.contains("plasma");
    m_useKdeHotkey = isKde;
    qDebug() << "TrayHost: platform=" << platform << "desktop=" << desktop << "useKDE=" << m_useKdeHotkey;

    Settings& settings = Settings::instance();
    m_kdeHotkey = std::make_unique<KGlobalAccelHotkey>();
    m_kdeHotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());

    m_portalHotkey = std::make_unique<PortalHotkey>();

    m_x11Hotkey = std::make_unique<GlobalHotkey>();
    m_x11Hotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());

    // Setup tray FIRST so even if hotkey fails we have a working tray icon
    setupTray();
    setupConnections();

    bool hotkeyInstalled = false;
    if (m_useKdeHotkey && m_kdeHotkey && m_kdeHotkey->install()) {
        hotkeyInstalled = true;
        qDebug() << "TrayHost: Global shortcut registered via KDE KGlobalAccel";
    } else if (m_portalHotkey && m_portalHotkey->install()) {
        hotkeyInstalled = true;
        qDebug() << "TrayHost: Global shortcut registered via XDG Desktop Portal";
    } else if (m_x11Hotkey && m_x11Hotkey->install()) {
        hotkeyInstalled = true;
        qDebug() << "TrayHost: Global shortcut registered via X11 XGrabKey";
    }

    if (!hotkeyInstalled) {
        // Non-fatal: app continues in tray-only mode. User can left-click the
        // tray icon or use "Show Emoji Panel" from the context menu, or bind 'EmojiTyper --toggle'.
        qWarning() << "Failed to install global hotkey automatically (platform:" << platform << "desktop:" << desktop << ")";
        qWarning() << "EmojiTyper running with tray and CLI support ('EmojiTyper --toggle').";
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
    // High-resolution logo loading with Qt resource fallback
    QIcon icon(":/icons/emojityper.png");
    if (icon.isNull()) {
        icon = QIcon(":/icons/emojityper.svg");
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme("emojityper");
    }
    if (icon.isNull()) {
        QStringList paths = {
            QDir::homePath() + "/.local/share/icons/hicolor/512x512/apps/emojityper.png",
            QDir::homePath() + "/.local/share/icons/hicolor/scalable/apps/emojityper.svg",
            "/usr/share/icons/hicolor/512x512/apps/emojityper.png",
            "/usr/share/icons/hicolor/scalable/apps/emojityper.svg",
            "/usr/local/share/icons/hicolor/scalable/apps/emojityper.svg",
            "assets/icons/emojityper.png",
            "assets/icons/emojityper.svg"
        };
        for (const QString& p : paths) {
            if (QFile::exists(p)) {
                icon = QIcon(p);
                break;
            }
        }
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme("face-smile", QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    }

    if (qApp) {
        qApp->setWindowIcon(icon);
    }

    m_trayMenu = std::make_unique<QMenu>();
    m_trayMenu->setObjectName("trayMenu");

    Settings& settings = Settings::instance();
    m_showAction = m_trayMenu->addAction(icon, QString("Show Emoji Picker\t%1").arg(settings.hotkeySequence()));
    connect(m_showAction, &QAction::triggered, this, [this](){ showPanel(); });

    m_trayMenu->addSeparator();

    // Preferences action with settings gear icon
    QIcon prefIcon = QIcon::fromTheme("preferences-system");
    if (prefIcon.isNull()) {
        prefIcon = QIcon::fromTheme("settings-configure");
    }
    if (prefIcon.isNull()) {
        prefIcon = QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    m_prefAction = m_trayMenu->addAction(prefIcon, "Preferences...");
    connect(m_prefAction, &QAction::triggered, this, &TrayHost::onPreferences);

    QAction* aboutAction = m_trayMenu->addAction("About TypEmoji");
    connect(aboutAction, &QAction::triggered, this, &TrayHost::onAbout);

    m_trayMenu->addSeparator();

    QAction* quitAction = m_trayMenu->addAction("Exit");
    connect(quitAction, &QAction::triggered, this, &TrayHost::onQuit);

    m_trayMenu->setStyleSheet(
        "QMenu {"
        "  background-color: #2A2A2E;"
        "  color: #F2F2F4;"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 8px;"
        "  padding: 4px;"
        "}"
        "QMenu::item {"
        "  padding: 6px 18px 6px 20px;"
        "  border-radius: 6px;"
        "  font-size: 13px;"
        "}"
        "QMenu::item:selected {"
        "  background-color: #0078D4;"
        "  color: #FFFFFF;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: rgba(255, 255, 255, 0.08);"
        "  margin: 4px 6px;"
        "}"
    );

    m_trayIcon = std::make_unique<QSystemTrayIcon>();
    m_trayIcon->setIcon(icon);
    m_trayIcon->setContextMenu(m_trayMenu.get());
    m_trayIcon->setToolTip(QString("TypEmoji — Press %1 for emoji picker").arg(settings.hotkeySequence()));
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
    if (m_kdeHotkey) {
        connect(m_kdeHotkey.get(), &KGlobalAccelHotkey::hotkeyPressed, this, &TrayHost::onHotkeyPressed);
    }
    if (m_portalHotkey) {
        connect(m_portalHotkey.get(), &PortalHotkey::hotkeyPressed, this, &TrayHost::onHotkeyPressed);
    }
    if (m_x11Hotkey) {
        connect(m_x11Hotkey.get(), &GlobalHotkey::hotkeyPressed, this, &TrayHost::onHotkeyPressed);
    }

    Settings& settings = Settings::instance();
    connect(&settings, &Settings::hotkeyChanged, this, &TrayHost::onHotkeySettingsChanged);
    connect(&settings, &Settings::themeChanged, this, &TrayHost::onThemeSettingsChanged);
}

void TrayHost::createPanel()
{
    if (!m_panel) {
        m_panel = std::make_unique<EmojiPanel>(m_database.get(), m_injector.get(), m_targetState.get());
        connect(m_panel.get(), &EmojiPanel::panelClosed, this, &TrayHost::onPanelClosed);
    }
}

bool TrayHost::isBinaryUpdated() const
{
    // Check if Linux kernel marked /proc/self/exe as deleted (binary replaced on disk)
    if (QFile::symLinkTarget("/proc/self/exe").endsWith(" (deleted)")) {
        return true;
    }
    QString appPath = QCoreApplication::applicationFilePath();
    QFileInfo fi(appPath);
    if (fi.exists() && m_binaryModTimeMs > 0 && fi.lastModified().toMSecsSinceEpoch() > m_binaryModTimeMs) {
        return true;
    }
    return false;
}

void TrayHost::reexecSelf(const QPoint& pos)
{
    qDebug() << "TrayHost: Seamlessly reloading updated binary...";
    if (m_kdeHotkey) {
        m_kdeHotkey->uninstall();
    }
    if (m_portalHotkey) {
        m_portalHotkey->uninstall();
    }
    if (m_x11Hotkey) {
        m_x11Hotkey->uninstall();
    }
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    if (m_panel) {
        m_panel->hide();
    }

    QString appPath = QCoreApplication::applicationFilePath();
    QByteArray appPathBytes = appPath.toLocal8Bit();

    QByteArray showArg = (pos.x() >= 0 || pos.y() >= 0 || (m_panel && m_panel->isVisible())) ? "--show" : "";
    char* argv[3];
    argv[0] = appPathBytes.data();
    if (!showArg.isEmpty()) {
        argv[1] = showArg.data();
        argv[2] = nullptr;
    } else {
        argv[1] = nullptr;
    }

    ::execv(appPathBytes.constData(), argv);
}

void TrayHost::showPanel(const QPoint& pos)
{
    if (isBinaryUpdated()) {
        reexecSelf(pos);
        return;
    }
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

void TrayHost::togglePanel()
{
    showPanel();
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

void TrayHost::onPreferences()
{
    if (!m_settingsDialog) {
        m_settingsDialog = std::make_unique<SettingsDialog>();
    }
    m_settingsDialog->loadSettings();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void TrayHost::onAbout()
{
    if (!m_settingsDialog) {
        m_settingsDialog = std::make_unique<SettingsDialog>();
    }
    m_settingsDialog->loadSettings();
    m_settingsDialog->showAboutTab();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void TrayHost::onHotkeySettingsChanged()
{
    Settings& settings = Settings::instance();
    qDebug() << "TrayHost: Hotkey configuration updated live:" << settings.hotkeySequence();

    if (m_kdeHotkey) {
        m_kdeHotkey->uninstall();
        m_kdeHotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());
        if (m_useKdeHotkey) {
            m_kdeHotkey->install();
        }
    }
    if (m_x11Hotkey) {
        m_x11Hotkey->uninstall();
        m_x11Hotkey->setHotkey(settings.hotkeyModifiers(), settings.hotkeyKey());
        if (!m_useKdeHotkey) {
            m_x11Hotkey->install();
        }
    }

    if (m_showAction) {
        m_showAction->setText(QString("Show Emoji Picker\t%1").arg(settings.hotkeySequence()));
    }
    if (m_trayIcon) {
        m_trayIcon->setToolTip(QString("TypEmoji — Press %1 for emoji picker").arg(settings.hotkeySequence()));
    }
}

void TrayHost::onThemeSettingsChanged()
{
    qDebug() << "TrayHost: Theme updated live, refreshing EmojiPanel";
    if (m_panel) {
        bool wasVisible = m_panel->isVisible();
        m_panel.reset();
        createPanel();
        if (wasVisible) {
            m_panel->showAt();
        }
    }
}

void TrayHost::onQuit() { quit(); }
void TrayHost::quit()
{
    if (m_settingsDialog) {
        m_settingsDialog->hide();
    }
    if (m_kdeHotkey) {
        m_kdeHotkey->uninstall();
    }
    if (m_portalHotkey) {
        m_portalHotkey->uninstall();
    }
    if (m_x11Hotkey) {
        m_x11Hotkey->uninstall();
    }
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    QCoreApplication::quit();
}
