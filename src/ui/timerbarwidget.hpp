#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include <QWidget>

#include "texture.hpp"

// Draws a horizontal "draining" progress bar representing the remaining
// notification timeout. Animates to zero over the notification's lifetime.
class TimerBarWidget : public QWidget {
  Q_OBJECT
public:
  explicit TimerBarWidget(QWidget *parent = nullptr);

  void start(int timeoutMs);
  void stop();
  // Freezes the countdown at its current remaining time (hover). resume()
  // continues from exactly where it left off, not a fresh full duration.
  void pause();
  void resume();

  QColor barColor() const { return m_barColor; }
  void setBarColor(const QColor &c) {
    m_barColor = c;
    update();
  }

  // Optional texture for the bar's filled portion, drawn instead of
  // barColor. Animated sources cycle frames using the bar's own repaint
  // timer while a countdown is active.
  void setBarImage(const QString &path) {
    m_frames = loadTextureFrames(path);
    update();
  }

  // Flush/edge look (bar_style=edge): no inset, square corners, spans the
  // full widget width. Default is inset+rounded, sitting inside the card's
  // padding like a normal progress bar.
  void setEdgeStyle(bool edge) {
    m_edgeStyle = edge;
    update();
  }

  QSize sizeHint() const override { return QSize(340, 6); }

protected:
  void paintEvent(QPaintEvent *event) override;

private slots:
  void onTick();

private:
  int currentFrameIndex() const;
  qint64 effectiveElapsed() const;

  QTimer m_timer;
  QElapsedTimer m_elapsed;
  qint64 m_durationMs = 0;
  QColor m_barColor{0x89, 0xb4, 0xfa};
  QColor m_trackColor{0x31, 0x32, 0x44};
  QList<TextureFrame> m_frames;
  bool m_paused = false;
  qint64 m_pauseStart = 0;
  qint64 m_totalPauseMs = 0;
  bool m_edgeStyle = false;
};
