#include "notificationserver.hpp"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QIcon>
#include <QDebug>

#include "../ui/notificationmanager.hpp"

NotificationServer::NotificationServer(QObject* parent)
    : QObject(parent) {
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
        bus.interface()->call(QDBus::Block,
                              QStringLiteral("RequestName"),
                              name,
                              QVariant::fromValue(static_cast<uint>(0x04))); // DBUS_NAME_FLAG_REPLACE_EXISTING
    if (!reply.isValid()) {
        qWarning() << "Failed to acquire" << name << ":" << reply.error().message();
        return false;
    }
    const uint result = reply.value();
    // 1 = PRIMARY_OWNER, 4 = IN_QUEUE, 2 = ALREADY_OWNER
    if (result != 1 && result != 2) {
        qWarning() << "Name" << name << "held by another owner (result" << result << ")";
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
    return {QStringLiteral("body"),         QStringLiteral("actions"),
            QStringLiteral("urgency"),      QStringLiteral("body-markup"),
            QStringLiteral("icon-static"),  QStringLiteral("persistence")};
}

QString NotificationServer::GetServerInformation(QString& vendor, QString& version,
                                                 QString& specVersion) {
    vendor = QStringLiteral("notifier");
    version = QStringLiteral("0.1.0");
    specVersion = QStringLiteral("1.2");
    return QStringLiteral("notifier");
}

uint NotificationServer::Notify(const QString& appName, uint replacesId,
                                const QString& appIcon, const QString& summary,
                                const QString& body, const QStringList& actions,
                                const QVariantMap& hints, int expireTimeout) {
    Q_UNUSED(replacesId);

    Notification n;
    n.id = m_nextId++;
    n.appName = appName;
    n.summary = summary;
    n.body = body;

    if (!appIcon.isEmpty()) {
        n.icon = QIcon::fromTheme(appIcon);
        if (n.icon.isNull()) {
            n.icon = QIcon(appIcon);
        }
    }

    bool ok = false;
    int urgency = hints.value(QStringLiteral("urgency")).toInt(&ok);
    n.urgency = ok ? urgency : 1;

    n.persist = (expireTimeout == -1) ||
                hints.value(QStringLiteral("persistence")).toBool();

    int timeout;
    if (n.persist) {
        // -1 or persistence hint: no timeout, stays until dismissed.
        timeout = -1;
    } else if (expireTimeout > 0) {
        // App gave an explicit positive duration: honor it.
        timeout = expireTimeout;
    } else {
        // 0 (server decides): pick a default based on urgency.
        switch (n.urgency) {
        case 1:  timeout = m_timeoutNormalMs;    break;
        case 2:  timeout = m_timeoutCriticalMs; break;
        default: timeout = m_timeoutDefaultMs;   break; // low/unset
        }
    }
    n.timeoutMs = timeout;
    n.actions = actions;

    qInfo("notify: urgency=%d expire=%d timeout=%d persist=%d",
          n.urgency, expireTimeout, timeout, n.persist);

    m_active.insert(n.id);
    emit notificationReceived(n);
    return n.id;
}

void NotificationServer::CloseNotification(uint id) {
    emit notificationClosed(id, 3); // 3 = NotificationClosed
}

QVariantList NotificationServer::GetHistory() {
    QVariantList result;
    if (!m_manager) {
        return result;
    }
    const auto history = m_manager->history();
    result.reserve(history.size());
    for (const auto& e : history) {
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
