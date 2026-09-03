#pragma once

#include <QPointer>
#include <QWidget>

#include "../config.hpp"
#include "../notification.hpp"
#include "timerbarwidget.hpp"

class QLabel;
class QTimer;

// Single floating notification card: translucent rounded panel with a title
// and body. Timed notifications show a draining bar and auto-dismiss;
// persistent ones stay until clicked with no bar.
class NotificationWindow : public QWidget {
    Q_OBJECT
public:
    explicit NotificationWindow(const Notification& n, const Config& cfg, QWidget* parent = nullptr);

    // Top offset (px) from the screen top; used to stack multiple notifications.
    void setTopOffset(int topMargin);

signals:
    void dismissed(uint id);
    void resized();

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onTimeoutFinished();

private:
    void layoutContents(const Notification& n);
    void setupLayerShell();

    uint m_id;
    int m_remainingMs;
    Config m_cfg;
    UrgencyStyle m_style;
    QPointer<QTimer> m_lifeTimer;
    TimerBarWidget* m_timerBar = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_bodyLabel = nullptr;
};
