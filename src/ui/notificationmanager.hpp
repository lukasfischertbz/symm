#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>

#include "../config.hpp"
#include "../notification.hpp"

class NotificationWindow;
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

// Maintains a stack of NotificationWindow layer-shell surfaces (one per
// notification), offset vertically under the top margin, plus the history log.
class NotificationManager : public QObject {
  Q_OBJECT
public:
  explicit NotificationManager(const Config &cfg, QObject *parent = nullptr);

  void show(const Notification &n);
  void remove(uint id);
  // Update-in-place for a replaced notification (replacesId): refreshes the
  // live card with the same id, or the queued copy if it isn't on screen yet.
  void update(const Notification &n);

  QList<HistoryEntry> history() const { return m_history; }
  void clearHistory();
  void showHistoryWindow();

signals:
  void actionInvoked(uint id, const QString &key);

private:
  void trimHistory();
  void loadHistory();
  void saveHistory() const;
  void reflow();
  void displayNow(const Notification &n);
  void promoteFromQueue();

  Config m_cfg;
  bool m_removing = false;
  QList<QPointer<NotificationWindow>> m_windows;
  // Notifications waiting for a free slot (see config.maxVisible). Nothing
  // here has a NotificationWindow yet, so its auto-dismiss timer hasn't
  // started -- it only starts once the notification is actually promoted to
  // m_windows and shown.
  QList<Notification> m_pending;
  QList<HistoryEntry> m_history;
  QPointer<NotificationHistoryWindow> m_historyWindow;
};