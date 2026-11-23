/**
 * EmojiTyper - Direct emoji typing tool for Linux (X11/Wayland)
 * Main entry point
 */
#include <QtWidgets/QApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QDebug>

#include "TrayHost.h"
#include "SingleInstance.h"
#include "Settings.h"

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("EmojiTyper");
    app.setApplicationVersion("2.3.2");
    app.setOrganizationName("EmojiTyper");

    QIcon appIcon(":/icons/emojityper.png");
    if (appIcon.isNull()) {
        appIcon = QIcon(":/icons/emojityper.svg");
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    SingleInstance instance("EmojiTyperInstance");
    QStringList args = app.arguments();

    if (instance.isAnotherRunning()) {
        if (args.contains("--quit") || args.contains("-q")) {
            instance.sendMessage("quit");
        } else if (args.contains("--toggle") || args.contains("-t")) {
            instance.sendMessage("toggle");
        } else if (args.contains("--reload")) {
            instance.sendMessage("reload");
        } else {
            instance.sendMessage("show");
        }
        return 0;
    }

    if (args.contains("--quit") || args.contains("-q")) {
        return 0;
    }

    Settings::instance().load();

    TrayHost trayHost;
    if (!trayHost.initialize()) {
        qCritical() << "Failed to initialize EmojiTyper";
        return 1;
    }

    instance.setMessageHandler([&trayHost](const QString& msg) {
        if (msg == "show") {
            trayHost.showPanel();
        } else if (msg == "toggle") {
            trayHost.togglePanel();
        } else if (msg == "quit") {
            trayHost.quit();
        } else if (msg == "reload") {
            trayHost.reexecSelf();
        }
    });

    if (args.contains("--show") || args.contains("--toggle") || args.contains("-t")) {
        trayHost.showPanel();
    }

    return app.exec();
}