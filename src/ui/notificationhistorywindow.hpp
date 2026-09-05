#pragma once

#include <QWidget>

#include "../config.hpp"
#include "notificationmanager.hpp"

class QLabel;
class QVBoxLayout;

// Frameless translucent layer-shell overlay listing the notification history,
// anchored top-right of its output (so it follows workspace switches and, like
// a panel, sits above regular windows). The window's own paintEvent draws the
// panel background AND every card's rounded background and border directly,
// because child widgets do not composite their backgrounds over a
// WA_TranslucentBackground surface. Children only render text.
//
// Opened via the `symm history` command (ShowHistory D-Bus slot).
class NotificationHistoryWindow : public QWidget {
  Q_OBJECT
public:
  explicit NotificationHistoryWindow(const Config &cfg,
                                     QWidget *parent = nullptr);

  void setEntries(const QList<HistoryEntry> &entries);
  void showTopRight();

protected:
  void showEvent(QShowEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  void setupLayerShell();
  void applyPlacement();
  QWidget *buildEntry(const HistoryEntry &entry);
  void clearEntries();

  QRect cardRect(int i) const;

  Config m_cfg;
  QList<QRect> m_cardRects;
  QVBoxLayout *m_listLayout = nullptr;
};

// A single history entry row. Renders text only; its rounded background and
// border are drawn by the parent NotificationHistoryWindow::paintEvent.
class HistoryCard : public QWidget {
  Q_OBJECT
public:
  HistoryCard(const HistoryEntry &entry, const Config &cfg,
              QWidget *parent = nullptr);

private:
  HistoryEntry m_entry;
  Config m_cfg;
};
