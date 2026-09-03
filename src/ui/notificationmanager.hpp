#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include "../config.hpp"
#include "../notification.hpp"

class NotificationWindow;

// Tracks all active notification windows and stacks them vertically below the
// top-right anchor so they never overlap.
class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(const Config& cfg, QObject* parent = nullptr);

    void show(const Notification& n);

private slots:
    void onWindowResized();

private:
    void reflow();

    Config m_cfg;
    QList<QPointer<NotificationWindow>> m_windows;
    int m_gap = 8;
};
