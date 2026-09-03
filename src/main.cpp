#include <QApplication>
#include <QDebug>

#include <LayerShellQt/Shell>

#include "config.hpp"
#include "dbus/notificationserver.hpp"
#include "ui/notificationmanager.hpp"
#include "ui/notificationwindow.hpp"

int main(int argc, char* argv[]) {
    // Enable wlr-layer-shell BEFORE any windows are created so notifications
    // float as true overlay surfaces (not tiled windows).
    LayerShellQt::Shell::useLayerShell();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("symm"));
    app.setApplicationDisplayName(QStringLiteral("symm"));

    const Config cfg = Config::load();

    NotificationServer server(&app);
    server.setTimeouts(cfg.timeoutDefaultMs, cfg.timeoutNormalMs, cfg.timeoutCriticalMs);

    if (!server.acquireServiceName()) {
        qCritical() << "Could not take org.freedesktop.Notifications;"
                       " another daemon is running.";
        return 1;
    }
    if (!server.registerObject()) {
        qCritical() << "Could not register /org/freedesktop/Notifications";
        return 1;
    }

    NotificationManager manager(cfg, &app);

    QObject::connect(&server, &NotificationServer::notificationReceived,
                     &manager, &NotificationManager::show);

    return app.exec();
}
