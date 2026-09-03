#include "timerbarwidget.hpp"

#include <QPainter>

TimerBarWidget::TimerBarWidget(QWidget* parent)
    : QWidget(parent) {
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

void TimerBarWidget::stop() {
    m_timer.stop();
}

void TimerBarWidget::onTick() {
    if (m_durationMs <= 0) {
        update();
        return;
    }
    update(); // repaint uses the real elapsed time
}

void TimerBarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track = rect().adjusted(2, 1, -2, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(m_trackColor);
    p.drawRoundedRect(track, 3.0, 3.0);

    double remaining = 1.0;
    if (m_durationMs > 0) {
        remaining = 1.0 - static_cast<double>(m_elapsed.elapsed()) / m_durationMs;
        if (remaining < 0.0) {
            remaining = 0.0;
        }
    }

    if (remaining <= 0.0) {
        return;
    }

    QRectF fill = track;
    fill.setRight(track.left() + track.width() * remaining);
    p.setBrush(m_barColor);
    p.drawRoundedRect(fill, 3.0, 3.0);
}
