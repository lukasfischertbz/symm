#include "portal_screenshot.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QRandomGenerator>
#include <QVariantMap>

#include "responsewaiter.hpp"

void requestPortalScreenshot(ScreenshotCallback callback) {
  QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                        QStringLiteral("/org/freedesktop/portal/desktop"),
                        QStringLiteral("org.freedesktop.portal.Screenshot"),
                        QDBusConnection::sessionBus());
  if (!portal.isValid()) {
    callback(QImage());
    return;
  }

  QVariantMap options;
  options[QStringLiteral("interactive")] = false;
  options[QStringLiteral("handle_token")] = QStringLiteral("symm%1").arg(
      QRandomGenerator::global()->bounded(1000000));

  QDBusReply<QDBusObjectPath> reply =
      portal.call(QStringLiteral("Screenshot"), QString(), options);
  if (!reply.isValid()) {
    callback(QImage());
    return;
  }

  auto *waiter = new ResponseWaiter(std::move(callback));
  QDBusConnection::sessionBus().connect(
      QStringLiteral("org.freedesktop.portal.Desktop"), reply.value().path(),
      QStringLiteral("org.freedesktop.portal.Request"),
      QStringLiteral("Response"), waiter, SLOT(onResponse(uint, QVariantMap)));
}
