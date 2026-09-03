#pragma once

#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include "../config.hpp"
#include "../notification.hpp"
#include "timerbarwidget.hpp"

class QLabel;
class QPushButton;
class QTimer;
class QScreen;

// Single floating notification card: an independent layer-shell surface so the
// compositor can blur it as one rectangle (kitty-style frost). Each card is its
// own window; the manager stacks them vertically via margins. Timed
// notifications show a draining bar and auto-dismiss; persistent ones stay
// until clicked with no bar. Notifications carrying action keys get a row of
// buttons that emit actionInvoked(id, key) when clicked.
class NotificationWindow : public QWidget {
  Q_OBJECT
public:
  explicit NotificationWindow(const Notification &n, const Config &cfg,
                              QScreen *targetScreen = nullptr,
                              QWidget *parent = nullptr);

  // Top offset (px) from the screen top; used to stack multiple notifications.
  void setTopOffset(int topMargin);

  uint id() const { return m_id; }

signals:
  void dismissed(uint id);
  void actionInvoked(uint id, const QString &key);
  void resized();

protected:
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private slots:
  void onTimeoutFinished();
  void onActionClicked(const QString &key);

private:
  void layoutContents(const Notification &n);
  void layoutActions(const QStringList &actions);
  void setupLayerShell();
  void toggleExpand();
  void updateBlurPanel();

  uint m_id;
  int m_remainingMs;
  Config m_cfg;
  UrgencyStyle m_style;
  QPointer<QTimer> m_lifeTimer;
  TimerBarWidget *m_timerBar = nullptr;
  QLabel *m_iconLabel = nullptr;
  QLabel *m_summaryLabel = nullptr;
  QLabel *m_bodyLabel = nullptr;
  QList<QPushButton *> m_actionButtons;

  // "Details" (click to expand truncated body).
  QString m_fullBody;
  bool m_truncated = false;
  bool m_expanded = false;

  // Cached frosted-glass backdrop; regenerated on show/resize, not every
  // paint (a real screen grab + gaussian blur is too slow to do per-frame).
  QPixmap m_blurPanel;
};
