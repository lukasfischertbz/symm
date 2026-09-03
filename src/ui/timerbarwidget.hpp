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
    explicit TimerBarWidget(QWidget* parent = nullptr);

    void start(int timeoutMs);
    void stop();

    QColor barColor() const { return m_barColor; }
    void setBarColor(const QColor& c) { m_barColor = c; update(); }

    QSize sizeHint() const override { return QSize(340, 6); }

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onTick();

private:
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    qint64 m_durationMs = 0;
    QColor m_barColor{0x89, 0xb4, 0xfa};
    QColor m_trackColor{0x31, 0x32, 0x44};
};
