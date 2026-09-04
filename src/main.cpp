#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>

#include <LayerShellQt/Shell>

#include "config.hpp"
#include "dbus/notificationserver.hpp"
#include "ui/blur.hpp"
#include "ui/notificationmanager.hpp"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("symm"));
  QApplication::setApplicationDisplayName(QStringLiteral("symm"));

  // CLI subcommand: talk to the running daemon over D-Bus and exit.
  const QStringList args = QApplication::arguments();
  if (args.size() > 1) {
    const QString &cmd = args.at(1);
    QString method;
    if (cmd == QStringLiteral("history") ||
        cmd == QStringLiteral("show-history")) {
      method = QStringLiteral("ShowHistory");
    } else if (cmd == QStringLiteral("clear")) {
      method = QStringLiteral("ClearHistory");
    } else {
      qWarning() << "Unknown command:" << cmd;
      return 1;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), method);
    QDBusConnection::sessionBus().send(call);
    return 0;
  }

  // Enable wlr-layer-shell BEFORE any windows are created so notifications
  // float as true overlay surfaces (not tiled windows).
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
  LayerShellQt::Shell::useLayerShell();
#endif

  const Config cfg = Config::load();

  // Grab a "clean" full-screen backdrop once, before any notification
  // surfaces exist. Cards crop their own region out of this later; grabbing
  // per-card would capture the card itself (self-blur) or run at stale
  // pre-position coordinates (see blur.hpp).
  if (cfg.blurEnabled) {
    initBlurSource();
  }

  NotificationServer server(&app);
  server.setTimeouts(cfg.timeoutDefaultMs, cfg.timeoutNormalMs,
                     cfg.timeoutCriticalMs, cfg.persistOnMinusOne);

  if (!NotificationServer::acquireServiceName()) {
    qCritical() << "Could not take org.freedesktop.Notifications;"
                   " another daemon is running.";
    return 1;
  }
  if (!server.registerObject()) {
    qCritical() << "Could not register /org/freedesktop/Notifications";
    return 1;
  }

  NotificationManager manager(cfg, &app);
  server.setManager(&manager);

  QObject::connect(&server, &NotificationServer::notificationReceived, &manager,
                   &NotificationManager::show);

  QObject::connect(&manager, &NotificationManager::actionInvoked, &server,
                   &NotificationServer::notifyActionInvoked);

  return QApplication::exec();
}
