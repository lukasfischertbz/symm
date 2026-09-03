#include "notificationmanager.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QStandardPaths>

#include <algorithm>

#include "notificationwindow.hpp"

namespace {
QString historyFilePath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/symm/history.json");
}
} // namespace

NotificationManager::NotificationManager(const Config& cfg, QObject* parent)
    : QObject(parent),
      m_cfg(cfg) {
    loadHistory();
}

void NotificationManager::show(const Notification& n) {
    m_cfg = Config::load();

    HistoryEntry entry;
    entry.id = n.id;
    entry.appName = n.appName;
    entry.summary = n.summary;
    entry.body = n.body;
    entry.urgency = n.urgency;
    entry.timestamp = QDateTime::currentDateTime();
    m_history.prepend(entry);
    trimHistory();
    saveHistory();

    auto* win = new NotificationWindow(n, m_cfg); // parentless window
    m_windows.append(win);

    // Reflow when this window is destroyed or resized.
    QObject::connect(win, &QObject::destroyed, this,
                     [this](QObject* obj) {
                         m_windows.erase(
                             std::remove_if(m_windows.begin(), m_windows.end(),
                                            [obj](const QPointer<NotificationWindow>& p) {
                                                return static_cast<QObject*>(p.data()) == obj;
                                            }),
                             m_windows.end());
                         reflow();
                     });
    QObject::connect(win, &NotificationWindow::resized, this,
                     &NotificationManager::onWindowResized);

    // Reflow now and again once the window gets its final size on screen.
    reflow();
    QMetaObject::invokeMethod(this, &NotificationManager::reflow, Qt::QueuedConnection);
}

void NotificationManager::onWindowResized() {
    reflow();
}

void NotificationManager::reflow() {
    m_windows.erase(
        std::remove_if(m_windows.begin(), m_windows.end(),
                       [](const QPointer<NotificationWindow>& p) { return !p; }),
        m_windows.end());

    int top = m_cfg.top;
    for (const QPointer<NotificationWindow>& w : m_windows) {
        if (!w) {
            continue;
        }
        w->setTopOffset(top);
        top += w->height() + m_gap;
    }
}

void NotificationManager::trimHistory() {
    const int max = qMax(0, m_cfg.historyMaxEntries);
    while (m_history.size() > max) {
        m_history.removeLast();
    }
}

void NotificationManager::loadHistory() {
    QFile file(historyFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isArray()) {
        return;
    }

    m_history.clear();
    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        HistoryEntry e;
        e.id = static_cast<uint>(obj.value(QStringLiteral("id")).toInt());
        e.appName = obj.value(QStringLiteral("appName")).toString();
        e.summary = obj.value(QStringLiteral("summary")).toString();
        e.body = obj.value(QStringLiteral("body")).toString();
        e.urgency = obj.value(QStringLiteral("urgency")).toInt();
        e.timestamp = QDateTime::fromString(
            obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        m_history.append(e);
    }
}

void NotificationManager::saveHistory() const {
    QFileInfo info(historyFilePath());
    if (!info.dir().exists()) {
        info.dir().mkpath(QStringLiteral("."));
    }

    QFile file(historyFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    QJsonArray array;
    for (const HistoryEntry& e : m_history) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = static_cast<int>(e.id);
        obj[QStringLiteral("appName")] = e.appName;
        obj[QStringLiteral("summary")] = e.summary;
        obj[QStringLiteral("body")] = e.body;
        obj[QStringLiteral("urgency")] = e.urgency;
        obj[QStringLiteral("timestamp")] = e.timestamp.toString(Qt::ISODate);
        array.append(obj);
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    file.close();
}

void NotificationManager::clearHistory() {
    m_history.clear();
    saveHistory();
}
