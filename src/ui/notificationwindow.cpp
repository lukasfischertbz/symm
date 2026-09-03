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
#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include "blur.hpp"

NotificationWindow::NotificationWindow(const Notification &n, const Config &cfg,
                                       QScreen *targetScreen, QWidget *parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint),
      m_id(n.id), m_remainingMs(n.timeoutMs), m_cfg(cfg) {
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
  setWindowTitle(QStringLiteral("notifier"));

  setFixedWidth(m_cfg.width);

  layoutContents(n);

  auto *layout = static_cast<QVBoxLayout *>(this->layout());

  // A bar only makes sense when the notification auto-dismisses on a timer.
  // Notifications that stay until clicked (persistent, or no positive
  // timeout) must show no bar. The bar and the dismiss timer are tied so
  // they can never disagree.
  const bool timed = !n.persist && n.timeoutMs > 0;

  m_timerBar = new TimerBarWidget(this);
  m_timerBar->setBarColor(m_style.bar);
  m_timerBar->setMoveRight(m_cfg.barMoveRight);
  m_timerBar->setReverse(m_cfg.barReverse);
  m_timerBar->setFill(m_cfg.barFill);
  m_timerBar->setVisible(timed);
  layout->addWidget(m_timerBar);

  layout->activate();
  int contentH = layout->sizeHint().height();
  setFixedSize(m_cfg.width, qMax(contentH, 70));

  if (timed) {
    m_lifeTimer = new QTimer(this);
    m_lifeTimer->setInterval(n.timeoutMs);
    connect(m_lifeTimer, &QTimer::timeout, this,
            &NotificationWindow::onTimeoutFinished);
    m_lifeTimer->start();
    m_timerBar->start(n.timeoutMs);
  }

  if (targetScreen != nullptr) {
    // Force native window creation now so we can pin it to a specific
    // output before the layer-shell surface is bound in showEvent().
    winId();
    if (QWindow *handle = windowHandle()) {
      handle->setScreen(targetScreen);
    }
  }
  show();
}

void NotificationWindow::setupLayerShell() {
  QWindow *handle = windowHandle();
  if (!handle) {
    return;
  }
  LayerShellQt::Window *shell = LayerShellQt::Window::get(handle);
  if (!shell) {
    return;
  }
  shell->setLayer(LayerShellQt::Window::LayerOverlay);
  shell->setScope(QStringLiteral("notifier"));
  using Anchor = LayerShellQt::Window::Anchor;
  shell->setAnchors(QFlags<Anchor>(Anchor::AnchorTop) |
                    QFlags<Anchor>(Anchor::AnchorRight));
  shell->setMargins(QMargins(0, m_cfg.top, m_cfg.margin, 0));
  shell->setExclusiveZone(-1); // no reserved space, floats over content
  shell->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
}

void NotificationWindow::showEvent(QShowEvent *event) {
  setupLayerShell();
  updateBlurPanel();
  QWidget::showEvent(event);
}

void NotificationWindow::updateBlurPanel() {
  if (!m_cfg.blurEnabled || size().isEmpty()) {
    m_blurPanel = QPixmap();
    return;
  }
  // A real screen grab + gaussian blur is too slow to redo every paint, so
  // it's cached here and only regenerated on show/resize/expand.
  const QRect globalRect(mapToGlobal(QPoint(0, 0)), size());
  m_blurPanel =
      makeFrostedPanel(globalRect, m_cfg.blurRadius, m_cfg.background);
  update();
}

void NotificationWindow::setTopOffset(int topMargin) {
  const int m = m_cfg.margin;
  if (QWindow *handle = windowHandle()) {
    if (LayerShellQt::Window *shell = LayerShellQt::Window::get(handle)) {
      shell->setMargins(QMargins(0, topMargin, m, 0));
    }
  }
}

void NotificationWindow::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateBlurPanel();
  emit resized();
}

void NotificationWindow::mousePressEvent(QMouseEvent *event) {
  // If the body is truncated, the first click expands it ("Details") instead
  // of dismissing, so you can actually read it. Once expanded (or if there
  // was nothing to expand), a click dismisses as before.
  if (m_truncated && !m_expanded) {
    toggleExpand();
    QWidget::mousePressEvent(event);
    return;
  }
  emit dismissed(m_id);
  close();
  QWidget::mousePressEvent(event);
}

void NotificationWindow::toggleExpand() {
  if (!m_truncated || m_expanded || m_bodyLabel == nullptr) {
    return;
  }
  m_expanded = true;
  m_bodyLabel->setText(m_fullBody);

  // Reading takes longer than the timeout allows -- stop the countdown while
  // expanded rather than yanking the notification away mid-read.
  if (m_lifeTimer) {
    m_lifeTimer->stop();
  }
  if (m_timerBar) {
    m_timerBar->stop();
    m_timerBar->setVisible(false);
  }

  auto *layout = static_cast<QVBoxLayout *>(this->layout());
  layout->activate();
  const int contentH = layout->sizeHint().height();
  setFixedSize(m_cfg.width, qMax(contentH, 70));

  updateBlurPanel();
  emit resized();
}

void NotificationWindow::layoutContents(const Notification &n) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(m_cfg.paddingH, m_cfg.paddingV, m_cfg.paddingH,
                             m_cfg.paddingV);
  layout->setSpacing(m_cfg.gap);

  const QString dim = m_cfg.dimTextColor.name();
  const QString fg = m_cfg.textColor.name();

  // Icon (left) + summary/body (right), side by side.
  auto *row = new QHBoxLayout;
  row->setSpacing(m_cfg.gap + 4);

  if (m_cfg.iconsEnabled && !n.icon.isNull()) {
    m_iconLabel = new QLabel(this);
    const int sz = m_cfg.iconSize;
    m_iconLabel->setFixedSize(sz, sz);
    m_iconLabel->setPixmap(n.icon.pixmap(sz, sz));
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    row->addWidget(m_iconLabel, 0, Qt::AlignTop);
  }

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(m_cfg.gap);

  if (!n.summary.isEmpty()) {
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setText(n.summary);
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    f.setBold(true);
    m_summaryLabel->setFont(f);
    m_summaryLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(fg));
    m_summaryLabel->setWordWrap(true);
    textCol->addWidget(m_summaryLabel);
  }

  if (!n.body.isEmpty()) {
    m_fullBody = n.body;
    m_truncated = n.body.size() > qMax(0, m_cfg.bodyTruncateChars);

    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setTextFormat(Qt::PlainText);
    m_bodyLabel->setText(m_truncated
                             ? n.body.left(m_cfg.bodyTruncateChars).trimmed() +
                                   QStringLiteral("…")
                             : n.body);
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    m_bodyLabel->setFont(f);
    m_bodyLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(dim));
    m_bodyLabel->setWordWrap(true);
    textCol->addWidget(m_bodyLabel);
  }

  row->addLayout(textCol, 1);
  layout->addLayout(row);

  layoutActions(n.actions);
}

void NotificationWindow::layoutActions(const QStringList &actions) {
  if (actions.size() < 2) {
    return;
  }
  auto *layout = static_cast<QVBoxLayout *>(this->layout());
  if (layout == nullptr) {
    return;
  }
  auto *row = new QHBoxLayout;
  row->setSpacing(6);
  const QString accent = m_style.accent.name();
  for (int i = 0; i + 1 < actions.size(); i += 2) {
    const QString key = actions.at(i);
    const QString label = actions.at(i + 1);
    auto *btn = new QPushButton(label.isEmpty() ? key : label, this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        QStringLiteral(
            "QPushButton { color: %1; background: transparent; border: 1px "
            "solid %2;"
            " border-radius: 4px; padding: 3px 8px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.08); }"
            "QPushButton:pressed { background: rgba(255,255,255,0.15); }")
            .arg(accent, accent));
    connect(btn, &QPushButton::clicked, this,
            [this, key] { onActionClicked(key); });
    row->addWidget(btn);
    m_actionButtons.append(btn);
  }
  layout->addLayout(row);
}

void NotificationWindow::onActionClicked(const QString &key) {
  emit actionInvoked(m_id, key);
}

void NotificationWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPainterPath path;
  path.addRoundedRect(rect(), m_cfg.radius, m_cfg.radius);

  if (m_cfg.blurEnabled && !m_blurPanel.isNull()) {
    p.setClipPath(path);
    p.drawPixmap(0, 0, m_blurPanel);
    p.setClipping(false);

    // Tint over the frosted backdrop so text stays legible (kitty-style
    // frost: blurred content behind a translucent color wash, not raw blur).
    QColor tint = m_cfg.background;
    tint.setAlphaF(tint.alphaF() * m_cfg.backgroundOpacity * 0.55);
    p.fillPath(path, tint);
  } else {
    QColor fill = m_cfg.background;
    fill.setAlphaF(fill.alphaF() * m_cfg.backgroundOpacity);
    p.fillPath(path, fill);
  }

  QColor border = m_style.accent;
  border.setAlpha(150);
  p.setPen(QPen(border, 1));
  p.drawPath(path);
}

void NotificationWindow::onTimeoutFinished() {
  emit dismissed(m_id);
  close();
}
