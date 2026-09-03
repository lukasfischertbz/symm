#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>

#include "../config.hpp"
#include "../notification.hpp"

class NotificationContainer;
class NotificationCard;
class NotificationHistoryWindow;

// A recorded notification in the history log.
struct HistoryEntry {
  uint id;
  QString appName;
  QString summary;
  QString body;
  int urgency;
  QDateTime timestamp;
};

// Hosts a single NotificationContainer that stacks all active notifications as
// child cards (auto-reflowed by the layout), plus the history log.
class NotificationManager : public QObject {
  Q_OBJECT
public:
  explicit NotificationManager(const Config &cfg, QObject *parent = nullptr);

  void show(const Notification &n);
  void remove(uint id);

  QList<HistoryEntry> history() const { return m_history; }
  void clearHistory();
  void showHistoryWindow();

signals:
  void actionInvoked(uint id, const QString &key);

private:
  void trimHistory();
  void loadHistory();
  void saveHistory() const;

  Config m_cfg;
  bool m_removing = false;
  QPointer<NotificationContainer> m_container;
  QList<QPointer<NotificationCard>> m_cards;
  QList<HistoryEntry> m_history;
  QPointer<NotificationHistoryWindow> m_historyWindow;
};