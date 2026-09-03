#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

// Draws a horizontal "draining" progress bar representing the remaining
// notification timeout. Animates to zero over the notification's lifetime.
class TimerBarWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimerBarWidget(QWidget *parent = nullptr);

  void start(int timeoutMs);
  void stop();

  QColor barColor() const { return m_barColor; }
  void setBarColor(const QColor &c) {
    m_barColor = c;
    update();
  }

  bool moveRight() const { return m_moveRight; }
  void setMoveRight(bool value) {
    m_moveRight = value;
    update();
  }

  bool reverse() const { return m_reverse; }
  void setReverse(bool value) {
    m_reverse = value;
    update();
  }

  // true: drain from full to empty; false: grow from empty to full.
  bool fill() const { return m_fill; }
  void setFill(bool value) {
    m_fill = value;
    update();
  }

  QSize sizeHint() const override { return QSize(340, 6); }

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void onTick();

private:
  QTimer m_timer;
  QElapsedTimer m_elapsed;
  qint64 m_durationMs = 0;
  bool m_moveRight = false;
  bool m_reverse = false;
  bool m_fill = true;
  QColor m_barColor{0x89, 0xb4, 0xfa};
  QColor m_trackColor{0x31, 0x32, 0x44};
};
