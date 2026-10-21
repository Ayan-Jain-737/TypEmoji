/*
 * Minimal KGlobalAccelD daemon executable - DEBUG VERSION
 */
#include <kglobalacceld.h>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("kglobalacceld");
    app.setOrganizationName("KDE");

    std::cerr << "[kglobalacceld] Starting KGlobalAccel daemon..." << std::endl;
    std::cerr << "[kglobalacceld] Session bus connected: " << QDBusConnection::sessionBus().isConnected() << std::endl;

    // Create the KGlobalAccelD instance - this registers the org.kde.KGlobalAccel service
    KGlobalAccelD daemon;
    
    std::cerr << "[kglobalacceld] Calling init()..." << std::endl;
    bool ok = daemon.init();
    std::cerr << "[kglobalacceld] init() returned: " << ok << std::endl;
    
    if (!ok) {
        std::cerr << "[kglobalacceld] Failed to initialize KGlobalAccelD!" << std::endl;
        return 1;
    }

    std::cerr << "[kglobalacceld] KGlobalAccelD initialized successfully" << std::endl;
    std::cerr << "[kglobalacceld] Registered DBus service: org.kde.KGlobalAccel" << std::endl;

    // Also watch the lowercase variant that kded provides
    QDBusServiceWatcher watcher1("org.kde.KGlobalAccel", QDBusConnection::sessionBus(),
                                QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    QDBusServiceWatcher watcher2("org.kde.kglobalaccel", QDBusConnection::sessionBus(),
                                QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);

    auto watchFn = [&](const QString &service) {
        std::cerr << "[kglobalacceld] Service changed: " << service.toStdString() << std::endl;
    };

    QObject::connect(&watcher1, &QDBusServiceWatcher::serviceUnregistered, watchFn);
    QObject::connect(&watcher1, &QDBusServiceWatcher::serviceRegistered, watchFn);
    QObject::connect(&watcher2, &QDBusServiceWatcher::serviceUnregistered, watchFn);
    QObject::connect(&watcher2, &QDBusServiceWatcher::serviceRegistered, watchFn);

    // Keep running
    std::cerr << "[kglobalacceld] Daemon running, waiting for shortcut invocations..." << std::endl;
    return app.exec();
}
