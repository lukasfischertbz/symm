#pragma once

#include <QPointer>
#include <QWidget>

#include "../config.hpp"

class NotificationCard;
class QVBoxLayout;

// Single top-right layer-shell window that hosts every active notification as
// stacked child cards. Its own paintEvent draws each card's rounded background
// and border (child widgets do not composite their backgrounds reliably over a
// translucent layer-shell surface). Adding/removing cards reflows them
// automatically via the layout — no per-window repositioning needed.
class NotificationContainer : public QWidget {
  Q_OBJECT
public:
  explicit NotificationContainer(const Config &cfg, QWidget *parent = nullptr);

  void addCard(NotificationCard *card);
  void removeCard(NotificationCard *card);

signals:
  void sizeChanged();

protected:
  void showEvent(QShowEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void setupLayerShell();
  void updateSize();

  Config m_cfg;
  QVBoxLayout *m_layout = nullptr;
};