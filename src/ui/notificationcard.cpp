#include "notificationcard.hpp"

#include <QBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>

NotificationCard::NotificationCard(const Notification &n, const Config &cfg,
                                   QWidget *parent)
    : QWidget(parent), m_id(n.id), m_cfg(cfg) {
  // Accent/border color from the notification's urgency.
  const QString key = Config::urgencyColorKey(n.urgency);
  if (key == QStringLiteral("low")) {
    m_style = m_cfg.low;
  } else if (key == QStringLiteral("critical")) {
    m_style = m_cfg.critical;
  } else {
    m_style = m_cfg.normal;
  }

  setAttribute(Qt::WA_TranslucentBackground);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::transparent);
  setPalette(pal);

  layoutContents(n);

  // A bar only makes sense when the notification auto-dismisses on a timer.
  // Persistent notifications (stay until clicked) show no bar at all.
  m_timed = !n.persist && n.timeoutMs > 0;

  auto *layout = static_cast<QVBoxLayout *>(this->layout());
  m_timerBar = new TimerBarWidget(this);
  m_timerBar->setBarColor(m_style.bar);
  m_timerBar->setVisible(m_timed);
  layout->addWidget(m_timerBar);

  layout->activate();
  setFixedWidth(m_cfg.width);
  // Keep the natural content height; never stretch or squeeze to fill the
  // container (which grows as cards are added).
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  if (m_timed) {
    m_lifeTimer = new QTimer(this);
    m_lifeTimer->setInterval(n.timeoutMs);
    connect(m_lifeTimer, &QTimer::timeout, this,
            &NotificationCard::onTimeoutFinished);
    m_lifeTimer->start();
    m_timerBar->start(n.timeoutMs);
  }
}

void NotificationCard::layoutContents(const Notification &n) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(m_cfg.paddingH, m_cfg.paddingV, m_cfg.paddingH,
                             m_cfg.paddingV);
  layout->setSpacing(m_cfg.gap);

  const QString dim = m_cfg.dimTextColor.name();
  const QString fg = m_cfg.textColor.name();

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
    layout->addWidget(m_summaryLabel);
  }

  if (!n.body.isEmpty()) {
    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setTextFormat(Qt::PlainText);
    m_bodyLabel->setText(n.body);
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    m_bodyLabel->setFont(f);
    m_bodyLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(dim));
    m_bodyLabel->setWordWrap(true);
    layout->addWidget(m_bodyLabel);
  }

  layoutActions(n.actions);
}

void NotificationCard::layoutActions(const QStringList &actions) {
  // The freedesktop actions array is [key, label, key, label, ...].
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

void NotificationCard::onActionClicked(const QString &key) {
  emit actionInvoked(m_id, key);
}

void NotificationCard::mousePressEvent(QMouseEvent *event) {
  // Clicking a notification dismisses it immediately.
  emit dismissed(m_id);
  QWidget::mousePressEvent(event);
}

void NotificationCard::onTimeoutFinished() {
  if (m_lifeTimer) {
    m_lifeTimer->stop();
  }
  emit dismissed(m_id);
}