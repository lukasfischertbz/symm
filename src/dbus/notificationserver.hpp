#pragma once

#include <QDBusContext>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSet>

#include "../notification.hpp"

class NotificationManager;

// Implements the freedesktop.org Notifications spec on the session bus.
class NotificationServer : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    explicit NotificationServer(QObject* parent = nullptr);

    void setTimeouts(int defaultMs, int normalMs, int criticalMs, bool persistMinusOne) {
        m_timeoutDefaultMs = defaultMs;
        m_timeoutNormalMs = normalMs;
        m_timeoutCriticalMs = criticalMs;
        m_persistOnMinusOne = persistMinusOne;
    }

    void setManager(NotificationManager* manager) { m_manager = manager; }

    // Claims the org.freedesktop.Notifications service name. Returns true on
    // success, false if the name is held by another daemon and cannot be taken.
    static bool acquireServiceName();

    // Registers /org/freedesktop/Notifications with this server instance.
    bool registerObject();

signals:
    void notificationReceived(const Notification& n);
    void notificationClosed(uint id, uint reason);

public slots:
    static QStringList GetCapabilities();
    static QString GetServerInformation(QString& vendor, QString& version, QString& specVersion);
    uint Notify(const QString& appName, uint replacesId, const QString& appIcon,
                const QString& summary, const QString& body,
                const QStringList& actions, const QVariantMap& hints, int expireTimeout);
    void CloseNotification(uint id);
    QVariantList GetHistory();
    void ShowHistory();
    void ClearHistory();

private:
    uint m_nextId = 1;
    int m_timeoutDefaultMs = 10000;
    int m_timeoutNormalMs = 5000;
    int m_timeoutCriticalMs = 15000;
    bool m_persistOnMinusOne = false;
    QSet<uint> m_active;
    NotificationManager* m_manager = nullptr;
};
