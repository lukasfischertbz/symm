#include "notificationmanager.hpp"

#include <QMetaObject>

#include <algorithm>

#include "notificationwindow.hpp"

NotificationManager::NotificationManager(const Config& cfg, QObject* parent)
    : QObject(parent),
      m_cfg(cfg) {
}

void NotificationManager::show(const Notification& n) {
    m_cfg = Config::load();
    auto* win = new NotificationWindow(n, m_cfg); // parentless window
    m_windows.append(win);

    // Reflow when this window is destroyed or resized.
    QObject::connect(win, &QObject::destroyed, this,
                     [this](QObject* obj) {
                         m_windows.erase(
                             std::remove_if(m_windows.begin(), m_windows.end(),
                                            [obj](const QPointer<NotificationWindow>& p) {
                                                return static_cast<QObject*>(p.data()) == obj;
                                            }),
                             m_windows.end());
                         reflow();
                     });
    QObject::connect(win, &NotificationWindow::resized, this,
                     &NotificationManager::onWindowResized);

    // Reflow now and again once the window gets its final size on screen.
    reflow();
    QMetaObject::invokeMethod(this, &NotificationManager::reflow, Qt::QueuedConnection);
}

void NotificationManager::onWindowResized() {
    reflow();
}

void NotificationManager::reflow() {
    m_windows.erase(
        std::remove_if(m_windows.begin(), m_windows.end(),
                       [](const QPointer<NotificationWindow>& p) { return !p; }),
        m_windows.end());

    int top = m_cfg.top;
    for (const QPointer<NotificationWindow>& w : m_windows) {
        if (!w) {
            continue;
        }
        w->setTopOffset(top);
        top += w->height() + m_gap;
    }
}
