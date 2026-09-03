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

    // Timeout precedence (what you asked: `-t` controls it, config is the default).
//   * expireTimeout > 0   -> exact auto-dismiss duration (CLI "off"). Never
//                            overridden by config.
//   * expireTimeout == -1 -> persistent IF persist_on_minus_one is on (CLI
//                            "on"). This is what `notify-send -t -1` sends, and
//                            also what a plain `notify-send` sends (no -t) —
//                            they are indistinguishable, so the config flag is
//                            the global default for that sentinel.
//   * expireTimeout == 0  -> server-decided urgency default.
n.persist = hints.value(QStringLiteral("persistence")).toBool();
if (expireTimeout > 0) {
    n.persist = false;
} else if (expireTimeout == -1 && m_persistOnMinusOne) {
    n.persist = true;
}
n.timeoutMs = 0;
if (n.persist) {
    n.timeoutMs = -1;
} else if (expireTimeout > 0) {
    n.timeoutMs = expireTimeout;
} else {
    // 0 / -1-with-persist-off: server-decided default based on urgency.
    switch (n.urgency) {
    case 1:  n.timeoutMs = m_timeoutNormalMs;    break;
    case 2:  n.timeoutMs = m_timeoutCriticalMs; break;
    default: n.timeoutMs = m_timeoutDefaultMs;   break; // low/unset
    }
}
n.actions = actions;

    qInfo("notify: urgency=%d expire=%d timeout=%lld persist=%d",
          n.urgency, expireTimeout, static_cast<long long>(n.timeoutMs), static_cast<int>(n.persist));

    m_active.insert(n.id);
    emit notificationReceived(n);
    return n.id;
}

void NotificationServer::CloseNotification(uint id) {
    emit notificationClosed(id, 3); // 3 = NotificationClosed
}

QVariantList NotificationServer::GetHistory() {
    QVariantList result;
    if (m_manager == nullptr) {
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
