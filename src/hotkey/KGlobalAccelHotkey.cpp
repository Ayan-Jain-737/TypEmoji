/**
 * KGlobalAccelHotkey implementation - KDE KGlobalAccel global hotkey for Wayland
 */
#include "KGlobalAccelHotkey.h"
#include <KGlobalAccel>
#include <KActionCollection>
#include <QKeySequence>
#include <QDebug>
#include <QtCore/QCoreApplication>
#include <QtWidgets/QApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <iostream>

KGlobalAccelHotkey::KGlobalAccelHotkey(QObject* parent) : QObject(parent)
{
}

KGlobalAccelHotkey::~KGlobalAccelHotkey()
{
    uninstall();
}

bool KGlobalAccelHotkey::install()
{
    if (m_installed) return true;
    
    setupAction();
    
    if (!m_action) return false;
    
    // Convert modifiers and keyCode to Qt::KeyboardModifiers and Qt::Key
    Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
    if (m_modifiers & 0x01) qtModifiers |= Qt::ShiftModifier;
    if (m_modifiers & 0x02) qtModifiers |= Qt::ControlModifier;
    if (m_modifiers & 0x04) qtModifiers |= Qt::AltModifier;
    if (m_modifiers & 0x08) qtModifiers |= Qt::MetaModifier;
    
    Qt::Key qtKey = static_cast<Qt::Key>(m_keyCode);
    
    // Register with KGlobalAccel using instance method with QAction pointer
    QKeySequence shortcut(qtModifiers | qtKey);
    
    KGlobalAccel* globalAccel = KGlobalAccel::self();
    if (!globalAccel) {
        qWarning() << "KGlobalAccel::self() returned null";
        std::cerr << "[KGlobalAccelHotkey] KGlobalAccel::self() returned null" << std::endl;
        return false;
    }
    
    // Set the shortcut using the instance method (NoAutoloading to ensure our shortcut is used)
    bool ok = globalAccel->setShortcut(m_action.get(), QList<QKeySequence>() << shortcut, KGlobalAccel::NoAutoloading);
    
    if (!ok) {
        qWarning() << "Failed to register global shortcut with KGlobalAccel:" << shortcut.toString();
        std::cerr << "[KGlobalAccelHotkey] Failed to register shortcut: " << shortcut.toString().toStdString() << std::endl;
        return false;
    }
    
    // Also set the default shortcut
    globalAccel->setDefaultShortcut(m_action.get(), QList<QKeySequence>() << shortcut, KGlobalAccel::NoAutoloading);
    
    // Connect the triggered signal - use Qt::QueuedConnection to ensure it runs in the event loop
    connect(m_action.get(), &QAction::triggered, this, [this](bool) {
        qDebug() << "KGlobalAccelHotkey: QAction::triggered signal received!";
        std::cerr << "[KGlobalAccelHotkey] QAction::triggered signal received!" << std::endl;
        emit hotkeyPressed(QPoint());
    }, Qt::QueuedConnection);
    
    // ALSO connect to the DBus signal from KGlobalAccel daemon(s)
    // KWin on Wayland emits the signal on org.kde.KWin, while kded emits on org.kde.kglobalaccel
    // We need to connect to BOTH to catch the signal regardless of which service emits it
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    QString componentName = "EmojiTyper";
    QString shortcutName = "show-panel";

    // Connect to kded's KGlobalAccel service (lowercase)
    bool slotConnected1 = sessionBus.connect(
        "org.kde.kglobalaccel",
        "/component/" + componentName,
        "org.kde.kglobalaccel.Component",
        "globalShortcutPressed",
        this,
        SLOT(onGlobalShortcutPressed(QString, QString, qlonglong))
    );

    // Connect to KWin's KGlobalAccel service (uppercase) - this is what Wayland uses!
    bool slotConnected2 = sessionBus.connect(
        "org.kde.KWin",
        "/component/" + componentName,
        "org.kde.kglobalaccel.Component",
        "globalShortcutPressed",
        this,
        SLOT(onGlobalShortcutPressed(QString, QString, qlonglong))
    );

    if (!slotConnected1 && !slotConnected2) {
        qWarning() << "Failed to connect to globalShortcutPressed DBus signal on either service";
        std::cerr << "[KGlobalAccelHotkey] Failed to connect to globalShortcutPressed DBus signal on either service" << std::endl;
    } else {
        if (slotConnected1) {
            qDebug() << "KGlobalAccelHotkey: Connected to globalShortcutPressed on org.kde.kglobalaccel";
            std::cerr << "[KGlobalAccelHotkey] Connected to globalShortcutPressed on org.kde.kglobalaccel" << std::endl;
        }
        if (slotConnected2) {
            qDebug() << "KGlobalAccelHotkey: Connected to globalShortcutPressed on org.kde.KWin";
            std::cerr << "[KGlobalAccelHotkey] Connected to globalShortcutPressed on org.kde.KWin" << std::endl;
        }
    }
    
    m_installed = true;
    qDebug() << "KGlobalAccelHotkey installed:" << shortcut.toString();
    std::cerr << "[KGlobalAccelHotkey] Installed: " << shortcut.toString().toStdString() << std::endl;
    return true;
}

void KGlobalAccelHotkey::uninstall()
{
    if (!m_installed) return;
    
    if (m_action) {
        // Unregister the shortcut using action
        KGlobalAccel* globalAccel = KGlobalAccel::self();
        if (globalAccel) {
            globalAccel->removeAllShortcuts(m_action.get());
        }
        
        m_action->disconnect();
        m_action.reset();
    }
    
    if (m_actionCollection) {
        m_actionCollection.reset();
    }
    
    // Disconnect DBus signal from both services
    QDBusConnection::sessionBus().disconnect(
        "org.kde.kglobalaccel",
        "/component/EmojiTyper",
        "org.kde.kglobalaccel.Component",
        "globalShortcutPressed",
        this,
        SLOT(onGlobalShortcutPressed(QString, QString, qlonglong))
    );
    
    QDBusConnection::sessionBus().disconnect(
        "org.kde.KWin",
        "/component/EmojiTyper",
        "org.kde.kglobalaccel.Component",
        "globalShortcutPressed",
        this,
        SLOT(onGlobalShortcutPressed(QString, QString, qlonglong))
    );
    
    m_installed = false;
    qDebug() << "KGlobalAccelHotkey uninstalled";
}

void KGlobalAccelHotkey::setHotkey(int modifiers, int keyCode)
{
    m_modifiers = modifiers;
    m_keyCode = keyCode;
    
    if (m_installed && m_action) {
        // Convert to Qt key sequence
        Qt::KeyboardModifiers qtModifiers = Qt::NoModifier;
        if (m_modifiers & 0x01) qtModifiers |= Qt::ShiftModifier;
        if (m_modifiers & 0x02) qtModifiers |= Qt::ControlModifier;
        if (m_modifiers & 0x04) qtModifiers |= Qt::AltModifier;
        if (m_modifiers & 0x08) qtModifiers |= Qt::MetaModifier;
        
        Qt::Key qtKey = static_cast<Qt::Key>(m_keyCode);
        QKeySequence shortcut(qtModifiers | qtKey);
        
        // Update the shortcut using the instance method with QAction pointer
        KGlobalAccel* globalAccel = KGlobalAccel::self();
        if (globalAccel) {
            bool ok = globalAccel->setShortcut(m_action.get(), QList<QKeySequence>() << shortcut, KGlobalAccel::NoAutoloading);
            globalAccel->setDefaultShortcut(m_action.get(), QList<QKeySequence>() << shortcut, KGlobalAccel::NoAutoloading);
            if (ok) {
                qDebug() << "KGlobalAccelHotkey updated:" << shortcut.toString();
            } else {
                qWarning() << "Failed to update KGlobalAccelHotkey:" << shortcut.toString();
            }
        }
    }
}

void KGlobalAccelHotkey::onGlobalShortcutPressed(const QString& component, const QString& shortcut, qlonglong timestamp)
{
    Q_UNUSED(component);
    Q_UNUSED(timestamp);
    if (shortcut == "show-panel") {
        qDebug() << "KGlobalAccelHotkey: globalShortcutPressed signal received via DBus!";
        std::cerr << "[KGlobalAccelHotkey] globalShortcutPressed signal received via DBus!" << std::endl;
        emit hotkeyPressed(QPoint());
    }
}

void KGlobalAccelHotkey::setupAction()
{
    // Create a KActionCollection to properly register the component with KGlobalAccel
    m_actionCollection = std::make_unique<KActionCollection>(qApp, QCoreApplication::applicationName());
    m_actionCollection->setComponentDisplayName(QCoreApplication::applicationName());
    m_actionCollection->setConfigGlobal(true);
    m_actionCollection->setComponentName(QCoreApplication::applicationName());
    
    // Create the action within the collection - this ensures proper component registration
    m_action = std::make_unique<QAction>(m_actionCollection.get());
    m_action->setObjectName(QStringLiteral("show-panel"));
    m_action->setText(QStringLiteral("Show Emoji Panel"));
    m_action->setEnabled(true);
    
    // Add action to collection - this should register with KGlobalAccel
    m_actionCollection->addAction(QStringLiteral("show-panel"), m_action.get());
    
    // Explicitly register the component with KGlobalAccel
    KGlobalAccel* globalAccel = KGlobalAccel::self();
    if (globalAccel) {
        // This triggers the component registration
        globalAccel->isComponentActive(QCoreApplication::applicationName());
    }
}