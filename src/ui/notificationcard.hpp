#pragma once

#include <QPointer>
#include <QWidget>

#include "../config.hpp"
#include "../notification.hpp"
#include "timerbarwidget.hpp"

class QLabel;
class QTimer;

// A single notification card, used as a child inside NotificationContainer.
// The container paints this card's rounded background and border because child
// widget backgrounds do not composite reliably over a translucent layer-shell
// parent; this widget renders text and the timer bar only.
//
// Timed notifications show a draining bar and auto-dismiss; persistent ones
// (which stay until clicked) show no bar.
class NotificationCard : public QWidget {
    Q_OBJECT
public:
    explicit NotificationCard(const Notification& n, const Config& cfg, QWidget* parent = nullptr);

    uint id() const { return m_id; }
    UrgencyStyle style() const { return m_style; }
    bool isTimed() const { return m_timed; }

signals:
    void dismissed(uint id);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onTimeoutFinished();

private:
    void layoutContents(const Notification& n);

    uint m_id;
    Config m_cfg;
    UrgencyStyle m_style;
    bool m_timed = false;
    QPointer<QTimer> m_lifeTimer;
    TimerBarWidget* m_timerBar = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_bodyLabel = nullptr;
};