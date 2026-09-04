#include "timerbarwidget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

TimerBarWidget::TimerBarWidget(QWidget *parent) : QWidget(parent) {
  setFixedHeight(6);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  connect(&m_timer, &QTimer::timeout, this, &TimerBarWidget::onTick);
}

qint64 TimerBarWidget::effectiveElapsed() const {
  return std::max<qint64>(0, m_elapsed.elapsed() - m_totalPauseMs);
}

void TimerBarWidget::start(int timeoutMs) {
  m_paused = false;
  m_totalPauseMs = 0;
  if (timeoutMs <= 0) {
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

void TimerBarWidget::stop() {
  m_timer.stop();
  m_totalPauseMs = 0;
}

void TimerBarWidget::pause() {
  if (m_durationMs <= 0 || m_paused) {
    return;
  }
  m_paused = true;
  m_pauseStart = m_elapsed.elapsed();
  m_timer.stop();
  update();
}

void TimerBarWidget::resume() {
  if (!m_paused) {
    return;
  }
  m_paused = false;
  m_totalPauseMs += m_elapsed.elapsed() - m_pauseStart;
  m_timer.start(16);
  update();
}

void TimerBarWidget::onTick() {
  if (m_durationMs <= 0) {
    update();
    return;
  }
  // Once the bar has fully drained/filled (no dismiss timer attached, e.g.
  // persistent notifications) stop ticking so it does not repaint forever.
  if (effectiveElapsed() >= m_durationMs) {
    m_timer.stop();
  }
  update(); // repaint uses the real elapsed time
}

void TimerBarWidget::paintEvent(QPaintEvent * /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const QRectF track =
      m_edgeStyle ? QRectF(rect()) : rect().adjusted(2, 1, -2, -1);
  const double radius = m_edgeStyle ? 0.0 : 3.0;
  p.setPen(Qt::NoPen);
  p.setBrush(m_trackColor);
  p.drawRoundedRect(track, radius, radius);

  double remaining = 1.0;
  if (m_durationMs > 0) {
    const double fraction = static_cast<double>(effectiveElapsed()) /
                            static_cast<double>(m_durationMs);
    // bar_fill: false shows remaining time (the bar drains), true fills up
    // with elapsed time instead (reverse fill, starting empty).
    remaining = m_fillUp ? fraction : (1.0 - fraction);
    remaining = std::clamp(remaining, 0.0, 1.0);
  }

  if (remaining <= 0.0) {
    return;
  }

  QRectF fill = track;
  // bar_move_right: fill pinned to the right edge by default (false), so as
  // it drains the accent part leaves the left side open; move_right = true
  // pins it to the left edge instead.
  if (m_moveRight) {
    fill.setRight(track.left() + (track.width() * remaining));
  } else {
    fill.setLeft(track.right() - (track.width() * remaining));
  }

  if (!m_frames.isEmpty()) {
    const int idx = currentFrameIndex();
    const QImage &img = m_frames[idx].image;
    QPainterPath fillPath;
    fillPath.addRoundedRect(fill, radius, radius);
    p.setClipPath(fillPath);
    p.drawImage(track, img, QRectF(0, 0, img.width(), img.height()));
    p.setClipping(false);
  } else {
    p.setBrush(m_barColor);
    p.drawRoundedRect(fill, radius, radius);
  }
}

int TimerBarWidget::currentFrameIndex() const {
  if (m_frames.size() <= 1) {
    return 0;
  }
  int total = 0;
  for (const TextureFrame &f : m_frames) {
    total += qMax(1, f.delayMs);
  }
  if (total <= 0) {
    return 0;
  }
  int t = static_cast<int>(effectiveElapsed()) % total;
  int acc = 0;
  for (int i = 0; i < m_frames.size(); ++i) {
    acc += qMax(1, m_frames[i].delayMs);
    if (t < acc) {
      return i;
    }
  }
  return static_cast<int>(m_frames.size()) - 1;
}
