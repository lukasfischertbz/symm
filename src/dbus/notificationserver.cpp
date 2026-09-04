#include "notificationserver.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDebug>
#include <QIcon>
#include <QImage>
#include <QPixmap>

#include "../ui/notificationmanager.hpp"

namespace {
// Decodes the freedesktop "image-data"/"image_data" hint: a raw pixel struct
// (iiibiiay) = width, height, rowstride, has_alpha, bits_per_sample,
// channels, pixel bytes. This is how most screenshot/media-key style tools
// (e.g. GNOME's volume OSD, some screenshot tools) send an icon instead of a
// themed icon name.
QImage imageFromHint(const QVariant &hint) {
  if (!hint.canConvert<QDBusArgument>()) {
    return {};
  }
  QDBusArgument arg = hint.value<QDBusArgument>();

  int width = 0;
  int height = 0;
  int rowstride = 0;
  bool hasAlpha = false;
  int bitsPerSample = 0;
  int channels = 0;
  QByteArray pixels;

  arg.beginStructure();
  arg >> width >> height >> rowstride >> hasAlpha >> bitsPerSample >> channels;
  arg >> pixels;
  arg.endStructure();

  if (width <= 0 || height <= 0 || pixels.isEmpty() || bitsPerSample != 8) {
    return {};
  }

  const QImage::Format fmt =
      hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
  const QImage view(reinterpret_cast<const uchar *>(pixels.constData()), width,
                    height, rowstride, fmt);
  // Deep copy: `pixels` (and thus the raw buffer QImage points at) goes out
  // of scope when this function returns.
  return view.copy();
}
} // namespace

NotificationServer::NotificationServer(QObject *parent) : QObject(parent) {
  // Allow returning a{sv} maps nested inside the history a(av) reply.
  qDBusRegisterMetaType<QVariantMap>();
}

bool NotificationServer::acquireServiceName() {
  QDBusConnection bus = QDBusConnection::sessionBus();
  if (!bus.isConnected()) {
    qWarning() << "Not connected to session bus";
    return false;
  }

  const QString name = QStringLiteral("org.freedesktop.Notifications");

  // Try to claim the name ourselves (we must be the active notifier).
  QDBusReply<uint> reply =
      bus.interface()->call(QDBus::Block, QStringLiteral("RequestName"), name,
                            QVariant::fromValue(static_cast<uint>(
                                0x04))); // DBUS_NAME_FLAG_REPLACE_EXISTING
  if (!reply.isValid()) {
    qWarning() << "Failed to acquire" << name << ":" << reply.error().message();
    return false;
  }
  const uint result = reply.value();
  // 1 = PRIMARY_OWNER, 4 = IN_QUEUE, 2 = ALREADY_OWNER
  if (result != 1 && result != 2) {
    qWarning() << "Name" << name << "held by another owner (result" << result
               << ")";
    return false;
  }
  return true;
}

bool NotificationServer::registerObject() {
  return QDBusConnection::sessionBus().registerObject(
      QStringLiteral("/org/freedesktop/Notifications"), this,
      QDBusConnection::ExportAllSlots);
}

QStringList NotificationServer::GetCapabilities() {
  return {QStringLiteral("body"),        QStringLiteral("actions"),
          QStringLiteral("urgency"),     QStringLiteral("body-markup"),
          QStringLiteral("icon-static"), QStringLiteral("persistence")};
}

QString NotificationServer::GetServerInformation(QString &vendor,
                                                 QString &version,
                                                 QString &specVersion) {
  vendor = QStringLiteral("notifier");
  version = QStringLiteral("0.1.0");
  specVersion = QStringLiteral("1.2");
  return QStringLiteral("notifier");
}

uint NotificationServer::Notify(const QString &appName, uint replacesId,
                                const QString &appIcon, const QString &summary,
                                const QString &body, const QStringList &actions,
                                const QVariantMap &hints, int expireTimeout) {
  Q_UNUSED(replacesId);

  Notification n;
  n.id = m_nextId++;
  n.appName = appName;
  n.summary = summary;
  n.body = body;

  // Icon precedence per the spec: raw pixel hints beat a path hint, which
  // beats the themed/path `app_icon` argument.
  QImage rawImage = imageFromHint(hints.value(QStringLiteral("image-data")));
  if (rawImage.isNull()) {
    rawImage = imageFromHint(
        hints.value(QStringLiteral("image_data"))); // deprecated alias
  }
  if (!rawImage.isNull()) {
    n.icon = QIcon(QPixmap::fromImage(rawImage));
  } else {
    const QString imagePath =
        hints.value(QStringLiteral("image-path")).toString();
    if (!imagePath.isEmpty()) {
      n.icon = QIcon(imagePath);
    }
    if (n.icon.isNull() && !appIcon.isEmpty()) {
      n.icon = QIcon::fromTheme(appIcon);
      if (n.icon.isNull()) {
        n.icon = QIcon(appIcon);
      }
    }
  }

  bool ok = false;
  int urgency = hints.value(QStringLiteral("urgency")).toInt(&ok);
  n.urgency = ok ? urgency : 1;

  // Timeout precedence per the freedesktop spec (`-t` wins, else the
  // sentinel rules):
  //   * expireTimeout > 0   -> exact auto-dismiss duration. Never overridden.
  //   * expireTimeout == 0  -> "never expires": stays until the user closes
  //                            it. This is what `notify-send -t 0` sends.
  //   * expireTimeout == -1 -> server decides: use this daemon's urgency
  //                            default. This is what a plain `notify-send`
  //                            sends (no -t), so it must NOT be treated as
  //                            persistent.
  // The nonstandard `persistence` hint (notify-send -h string:persistence:true)
  // also means "never expires", unless an explicit positive -t overrides it.
  n.persist = hints.value(QStringLiteral("persistence")).toBool();
  if (expireTimeout > 0) {
    n.persist = false;
    n.timeoutMs = expireTimeout;
  } else if (expireTimeout == 0) {
    n.persist = true;
    n.timeoutMs = -1;
  } else if (n.persist) {
    // expireTimeout == -1 plus the persistence hint: never expires.
    n.timeoutMs = -1;
  } else {
    // expireTimeout == -1: server-decided default based on urgency.
    switch (n.urgency) {
    case 1:
      n.timeoutMs = m_timeoutNormalMs;
      break;
    case 2:
      n.timeoutMs = m_timeoutCriticalMs;
      break;
    default:
      n.timeoutMs = m_timeoutDefaultMs;
      break; // low/unset
    }
  }
  n.actions = actions;

  qInfo("notify: urgency=%d expire=%d timeout=%lld persist=%d "
        "persistenceHint='%s'",
        n.urgency, expireTimeout, static_cast<long long>(n.timeoutMs),
        static_cast<int>(n.persist),
        hints.value(QStringLiteral("persistence"))
            .toString()
            .toUtf8()
            .constData());

  m_active.insert(n.id);
  emit notificationReceived(n);
  return n.id;
}

void NotificationServer::CloseNotification(uint id) {
  emit notificationClosed(id, 3); // 3 = NotificationClosed
}

void NotificationServer::notifyActionInvoked(uint id,
                                             const QString &actionKey) {
  // Freedesktop: notify the client that an action was chosen on its
  // notification. Broadcast on the session bus so notify-send & friends pick
  // it up via the org.freedesktop.Notifications.ActionInvoked signal.
  QDBusMessage signal = QDBusMessage::createSignal(
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("ActionInvoked"));
  signal << id << actionKey;
  QDBusConnection::sessionBus().send(signal);

  emit notificationClosed(id, 2); // 2 = dismissed by user action
}

QVariantList NotificationServer::GetHistory() {
  QVariantList result;
  if (m_manager == nullptr) {
    return result;
  }
  const auto history = m_manager->history();
  result.reserve(history.size());
  for (const auto &e : history) {
    QVariantMap map;
    map[QStringLiteral("id")] = static_cast<int>(e.id);
    map[QStringLiteral("appName")] = e.appName;
    map[QStringLiteral("summary")] = e.summary;
    map[QStringLiteral("body")] = e.body;
    map[QStringLiteral("urgency")] = e.urgency;
    map[QStringLiteral("timestamp")] = e.timestamp.toString(Qt::ISODate);
    result.append(map);
  }
  return result;
}

void NotificationServer::ShowHistory() {
  if (m_manager != nullptr) {
    m_manager->showHistoryWindow();
  }
}

void NotificationServer::ClearHistory() {
  if (m_manager != nullptr) {
    m_manager->clearHistory();
  }
}
