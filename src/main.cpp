#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QtGlobal>

#include "config.hpp"
#include "dbus/notificationserver.hpp"
#include "ui/notificationmanager.hpp"

int main(int argc, char *argv[]) {
  // Cards are layer-shell surfaces; tell Qt's Wayland integration to use the
  // layer-shell role (must happen before the platform is initialized).
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("symm"));
  QApplication::setApplicationDisplayName(QStringLiteral("symm"));

  // Cards are layer-shell overlays, which only exist on Wayland. Running under
  // another platform (e.g. xcb/XWayland from a stale environment or a manual
  // launch) silently degrades into plain windows that no longer float above
  // workspaces.
  if (!QGuiApplication::platformName().contains("wayland")) {
    qWarning() << "symm: running on" << QGuiApplication::platformName()
               << "- cards need the Wayland platform for layer-shell "
                  "placement. Start the daemon without QT_QPA_PLATFORM=xcb "
                  "(make run does this).";
  }

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

  const Config cfg = Config::load();
  // Per-notification entries re-read the config themselves (see
  // NotificationManager::show), so a theme switch applies without a restart.

  NotificationServer server(&app);
  server.setTimeouts(cfg.timeoutDefaultMs, cfg.timeoutNormalMs,
                     cfg.timeoutCriticalMs);

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
  QObject::connect(&server, &NotificationServer::notificationUpdated, &manager,
                   &NotificationManager::update);

  QObject::connect(&manager, &NotificationManager::actionInvoked, &server,
                   &NotificationServer::notifyActionInvoked);

  return QApplication::exec();
}
