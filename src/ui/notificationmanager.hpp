#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>

#include "../config.hpp"
#include "../notification.hpp"

class NotificationWindow;

// A recorded notification in the history log.
struct HistoryEntry {
    uint id;
    QString appName;
    QString summary;
    QString body;
    int urgency;
    QDateTime timestamp;
};

// Tracks all active notification windows and stacks them vertically below the
// top-right anchor so they never overlap.
class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(const Config& cfg, QObject* parent = nullptr);

    void show(const Notification& n);

    QList<HistoryEntry> history() const { return m_history; }
    void clearHistory();

private slots:
    void onWindowResized();

private:
    void reflow();
    void trimHistory();
    void loadHistory();
    void saveHistory() const;

    Config m_cfg;
    QList<QPointer<NotificationWindow>> m_windows;
    QList<HistoryEntry> m_history;
    int m_gap = 8;
};
