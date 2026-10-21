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
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EmojiTyper");

    SingleInstance instance("EmojiTyperInstance");
    if (instance.isAnotherRunning()) {
        qDebug() << "Another instance running, sending show message";
        instance.sendShowMessage();
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
        }
    });

    return app.exec();
}