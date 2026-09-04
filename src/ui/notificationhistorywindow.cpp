#include "notificationhistorywindow.hpp"

#include <LayerShellQt/Window>

#include <QBoxLayout>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

namespace {
QString relativeTimeString(const QDateTime &ts) {
  if (!ts.isValid()) {
    return {};
  }
  const qint64 secs = ts.secsTo(QDateTime::currentDateTime());
  if (secs < 5) {
    return QObject::tr("just now");
  }
  if (secs < 60) {
    return QObject::tr("%1s ago").arg(secs);
  }
  if (secs < 3600) {
    return QObject::tr("%1m ago").arg(secs / 60);
  }
  if (secs < 86400) {
    return QObject::tr("%1h ago").arg(secs / 3600);
  }
  if (secs < static_cast<qint64>(86400) * 7) {
    return QObject::tr("%1d ago").arg(secs / 86400);
  }
  return ts.toString(QStringLiteral("MMM d"));
}

// Single-line elision (QLabel has no built-in multi-line elide, and a
// compact list needs each entry to stay one line regardless of how long the
// summary/body was).
QString elide(const QString &text, const QFontMetrics &fm, int maxWidth) {
  return fm.elidedText(text.simplified(), Qt::ElideRight, maxWidth);
}
} // namespace

NotificationHistoryWindow::NotificationHistoryWindow(const Config &cfg,
                                                     QWidget *parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint),
      m_cfg(cfg) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_DeleteOnClose);

  setFixedWidth(m_cfg.width);
  setFixedHeight(500);

  auto *header = new QLabel(tr("Notification History"), this);
  QFont hf(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize) + 2);
  hf.setBold(true);
  header->setFont(hf);
  header->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                            .arg(m_cfg.textColor.name()));

  auto *clearBtn = new QPushButton(tr("Clear"), this);
  clearBtn->setCursor(Qt::PointingHandCursor);
  const QColor blend = m_cfg.background;
  clearBtn->setStyleSheet(
      QString("QPushButton {"
              "  color: %1;"
              "  background: rgba(%2,%3,%4,60);"
              "  border: 1px solid rgba(255,255,255,30);"
              "  border-radius: 6px;"
              "  padding: 4px 12px;"
              "} "
              "QPushButton:hover { background: rgba(%2,%3,%4,110); }")
          .arg(m_cfg.textColor.name())
          .arg(blend.red())
          .arg(blend.green())
          .arg(blend.blue()));
  connect(clearBtn, &QPushButton::clicked, this, [this] { clearEntries(); });

  auto *headerRow = new QHBoxLayout;
  headerRow->setContentsMargins(0, 0, 0, 0);
  headerRow->addWidget(header);
  headerRow->addStretch(1);
  headerRow->addWidget(clearBtn);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 12, 14, 14);
  layout->setSpacing(8);
  layout->addLayout(headerRow);
  layout->addStretch(1);
  m_listLayout = layout;
}

void NotificationHistoryWindow::setEntries(const QList<HistoryEntry> &entries) {
  clearEntries();
  const int count =
      static_cast<int>(qMin(entries.size(), qMax(1, m_cfg.historyRecentCount)));
  for (int i = 0; i < count; ++i) {
    m_listLayout->insertWidget(m_listLayout->count() - 1,
                               buildEntry(entries.at(i)));
  }
  const int rowH = 40; // approximate compact row height incl. spacing
  setFixedHeight(qMin(500, 60 + count * rowH));
}

void NotificationHistoryWindow::clearEntries() {
  if (m_listLayout == nullptr) {
    return;
  }
  // Keep item 0 (header row) and the trailing stretch (last item).
  for (int i = 1; i + 1 < m_listLayout->count();) {
    QLayoutItem *item = m_listLayout->takeAt(i);
    if (QWidget *w = item->widget()) {
      w->deleteLater();
    }
    delete item;
  }
}

QWidget *NotificationHistoryWindow::buildEntry(const HistoryEntry &entry) {
  return new HistoryCard(entry, m_cfg, this);
}

HistoryCard::HistoryCard(const HistoryEntry &entry, const Config &cfg,
                         QWidget *parent)
    : QWidget(parent), m_entry(entry), m_cfg(cfg) {
  setAttribute(Qt::WA_TranslucentBackground);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, Qt::transparent);
  setPalette(pal);

  const QString fg = m_cfg.textColor.name();
  const QString dim = m_cfg.dimTextColor.name();

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 6, 12, 6);
  layout->setSpacing(1);

  auto *titleRow = new QHBoxLayout;
  titleRow->setSpacing(8);

  auto *appLabel =
      new QLabel(entry.appName.isEmpty() ? tr("Unknown") : entry.appName, this);
  QFont af(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize) - 1);
  af.setBold(true);
  appLabel->setFont(af);
  appLabel->setStyleSheet(
      QStringLiteral("color: %1; background: transparent;").arg(fg));
  titleRow->addWidget(appLabel);
  titleRow->addStretch(1);

  auto *timeLabel = new QLabel(relativeTimeString(entry.timestamp), this);
  QFont tf(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize) - 1);
  timeLabel->setFont(tf);
  timeLabel->setStyleSheet(
      QStringLiteral("color: %1; background: transparent;").arg(dim));
  titleRow->addWidget(timeLabel);

  layout->addLayout(titleRow);

  const QString combined =
      entry.body.isEmpty()
          ? entry.summary
          : (entry.summary + QStringLiteral(" — ") + entry.body);
  if (!combined.isEmpty()) {
    auto *s = new QLabel(this);
    s->setTextFormat(Qt::PlainText);
    QFont sf(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize) - 1);
    s->setFont(sf);
    s->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(dim));
    const int textWidth = m_cfg.width - 2 * 12 - 2 * m_cfg.margin;
    s->setText(elide(combined, QFontMetrics(sf), qMax(80, textWidth)));
    layout->addWidget(s);
  }
}

void NotificationHistoryWindow::showTopRight() {
  show();
  raise();
}

void NotificationHistoryWindow::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  close();
}

void NotificationHistoryWindow::showEvent(QShowEvent *event) {
  setupLayerShell();
  QWidget::showEvent(event);
}

void NotificationHistoryWindow::setupLayerShell() {
  QWindow *handle = windowHandle();
  if (handle == nullptr) {
    return;
  }
  LayerShellQt::Window *shell = LayerShellQt::Window::get(handle);
  if (shell == nullptr) {
    return;
  }
  shell->setLayer(LayerShellQt::Window::LayerOverlay);
  using Anchor = LayerShellQt::Window::Anchor;
  shell->setAnchors(QFlags<Anchor>(Anchor::AnchorTop) |
                    QFlags<Anchor>(Anchor::AnchorRight));
  shell->setMargins(QMargins(0, m_cfg.top, m_cfg.margin, 0));
  shell->setExclusiveZone(-1);
  shell->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
}

void NotificationHistoryWindow::paintEvent(QPaintEvent * /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Container panel.
  QPainterPath panel;
  panel.addRoundedRect(rect(), m_cfg.radius, m_cfg.radius);
  p.fillPath(panel, m_cfg.background);
  QColor panelBorder = m_cfg.normal.accent;
  panelBorder.setAlpha(150);
  p.setPen(QPen(panelBorder, 1));
  p.drawPath(panel);

  // Each card's rounded background + border, drawn relative to this window.
  QColor bg = m_cfg.background;
  QColor cardBg = bg.lighter(112);
  cardBg.setAlpha(bg.alpha());
  QColor cardBorder = m_cfg.normal.accent;
  cardBorder.setAlpha(180);
  QPen cardPen(cardBorder, 2);

  if (m_listLayout == nullptr) {
    return;
  }
  for (int i = 0; i < m_listLayout->count(); ++i) {
    QLayoutItem *item = m_listLayout->itemAt(i);
    if ((item == nullptr) || (item->widget() == nullptr)) {
      continue;
    }
    if (qobject_cast<HistoryCard *>(item->widget()) == nullptr) {
      continue;
    }
    const QRect r = item->widget()->geometry();
    if (r.isEmpty()) {
      continue;
    }
    QPainterPath card;
    card.addRoundedRect(r, m_cfg.radius, m_cfg.radius);
    p.fillPath(card, cardBg);
    p.setPen(cardPen);
    p.drawPath(card);
  }
}
