#include "notificationwindow.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QBoxLayout>
#include <QFlags>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>

NotificationWindow::NotificationWindow(const Notification& n, const Config& cfg, QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint),
      m_id(n.id),
      m_remainingMs(n.timeoutMs),
      m_cfg(cfg) {
    // Pick the accent style for this notification's urgency.
    const QString key = m_cfg.urgencyColorKey(n.urgency);
    if (key == QStringLiteral("low")) {
        m_style = m_cfg.low;
    } else if (key == QStringLiteral("critical")) {
        m_style = m_cfg.critical;
    } else {
        m_style = m_cfg.normal;
    }

    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("notifier"));

    setFixedWidth(m_cfg.width);

    layoutContents(n);

    m_timerBar = new TimerBarWidget(this);
    m_timerBar->setBarColor(m_style.bar);

    auto* layout = static_cast<QVBoxLayout*>(this->layout());
    layout->addWidget(m_timerBar);

    layout->activate();
    int contentH = layout->sizeHint().height();
    setFixedSize(m_cfg.width, qMax(contentH, 70));

    if (!n.persist && n.timeoutMs > 0) {
        m_lifeTimer = new QTimer(this);
        m_lifeTimer->setInterval(n.timeoutMs);
        connect(m_lifeTimer, &QTimer::timeout, this, &NotificationWindow::onTimeoutFinished);
        m_lifeTimer->start();
    }

    // Timer bar: drain across the notification's lifetime, or stay full for
    // persistent notifications until the user dismisses them.
    m_timerBar->start(n.persist ? 0 : (n.timeoutMs > 0 ? n.timeoutMs : m_cfg.timerDefaultMs));
    show();
}

void NotificationWindow::setupLayerShell() {
    QWindow* handle = windowHandle();
    if (!handle) {
        return;
    }
    LayerShellQt::Window* shell = LayerShellQt::Window::get(handle);
    if (!shell) {
        return;
    }
    shell->setLayer(LayerShellQt::Window::LayerOverlay);
    using Anchor = LayerShellQt::Window::Anchor;
    shell->setAnchors(QFlags<Anchor>(Anchor::AnchorTop) |
                      QFlags<Anchor>(Anchor::AnchorRight));
    shell->setMargins(QMargins(0, m_cfg.top, m_cfg.margin, 0));
    shell->setExclusiveZone(-1); // no reserved space, floats over content
    shell->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
}

void NotificationWindow::showEvent(QShowEvent* event) {
    setupLayerShell();
    QWidget::showEvent(event);
}

void NotificationWindow::setTopOffset(int topMargin) {
    const int m = m_cfg.margin;
    if (QWindow* handle = windowHandle()) {
        if (LayerShellQt::Window* shell = LayerShellQt::Window::get(handle)) {
            shell->setMargins(QMargins(0, topMargin, m, 0));
        }
    }
}

void NotificationWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit resized();
}

void NotificationWindow::mousePressEvent(QMouseEvent* event) {
    // Clicking any notification dismisses it immediately.
    emit dismissed(m_id);
    close();
    QWidget::mousePressEvent(event);
}

void NotificationWindow::layoutContents(const Notification& n) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(2);

    const QString dim = m_cfg.dimTextColor.name();
    const QString fg = m_cfg.textColor.name();

    if (!n.summary.isEmpty()) {
        m_summaryLabel = new QLabel(this);
        m_summaryLabel->setTextFormat(Qt::PlainText);
        m_summaryLabel->setText(n.summary);
        QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
        f.setBold(true);
        m_summaryLabel->setFont(f);
        m_summaryLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(fg));
        m_summaryLabel->setWordWrap(true);
        layout->addWidget(m_summaryLabel);
    }

    if (!n.body.isEmpty()) {
        m_bodyLabel = new QLabel(this);
        m_bodyLabel->setTextFormat(Qt::PlainText);
        m_bodyLabel->setText(n.body);
        QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
        m_bodyLabel->setFont(f);
        m_bodyLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(dim));
        m_bodyLabel->setWordWrap(true);
        layout->addWidget(m_bodyLabel);
    }
}

void NotificationWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), m_cfg.radius, m_cfg.radius);

    p.fillPath(path, m_cfg.background);

    QColor border = m_style.accent;
    border.setAlpha(150);
    p.setPen(QPen(border, 1));
    p.drawPath(path);
}

void NotificationWindow::onTimeoutFinished() {
    emit dismissed(m_id);
    close();
}
