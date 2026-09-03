#include "timerbarwidget.hpp"

#include <QPainter>
#include <algorithm>

TimerBarWidget::TimerBarWidget(QWidget *parent) : QWidget(parent) {
  setFixedHeight(6);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  connect(&m_timer, &QTimer::timeout, this, &TimerBarWidget::onTick);
}

void TimerBarWidget::start(int timeoutMs) {
  if (timeoutMs <= 0) {
    // Permanent notification: keep the bar full.
    m_durationMs = 0;
    m_timer.stop();
    update();
    return;
  }
  m_durationMs = timeoutMs;
  m_elapsed.restart();
  m_timer.start(16); // ~60fps
  update();
}

void TimerBarWidget::stop() { m_timer.stop(); }

void TimerBarWidget::onTick() {
  if (m_durationMs <= 0) {
    update();
    return;
  }
  update(); // repaint uses the real elapsed time
}

void TimerBarWidget::paintEvent(QPaintEvent * /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF track = rect().adjusted(2, 1, -2, -1);
  p.setPen(Qt::NoPen);
  p.setBrush(m_trackColor);
  p.drawRoundedRect(track, 3.0, 3.0);

  double fillRatio = 1.0;
  if (m_durationMs > 0) {
    const double elapsed = static_cast<double>(m_elapsed.elapsed());
    const double elapsedRatio =
        std::clamp(elapsed / static_cast<double>(m_durationMs), 0.0, 1.0);
    const double remainingRatio = 1.0 - elapsedRatio;
    fillRatio = m_fill ? elapsedRatio : remainingRatio;
    if (m_reverse) {
      fillRatio = 1.0 - fillRatio;
    }
    fillRatio = std::clamp(fillRatio, 0.0, 1.0);
  }

  if (fillRatio <= 0.0) {
    return;
  }

  QRectF fill = track;
  const double fillWidth = track.width() * fillRatio;
  if (m_moveRight) {
    fill.setLeft(track.left());
    fill.setRight(track.left() + fillWidth);
  } else {
    fill.setLeft(track.right() - fillWidth);
    fill.setRight(track.right());
  }

  p.setBrush(m_barColor);
  p.drawRoundedRect(fill, 3.0, 3.0);
}
