#include "notificationcontainer.hpp"

#include <LayerShellQt/Window>

#include <QBoxLayout>
#include <QFlags>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include "notificationcard.hpp"

NotificationContainer::NotificationContainer(const Config &cfg, QWidget *parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint),
      m_cfg(cfg) {
  setAttribute(Qt::WA_TranslucentBackground);
  setWindowTitle(QStringLiteral("notifier"));

  setFixedWidth(m_cfg.width);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(m_cfg.cardSpacing);
}

void NotificationContainer::addCard(NotificationCard *card) {
  m_layout->addWidget(card);
  updateSize();
}

void NotificationContainer::removeCard(NotificationCard *card) {
  m_layout->removeWidget(card);
  card->deleteLater();
  updateSize();
}

void NotificationContainer::updateSize() {
  m_layout->activate();
  // Sum the natural heights of the cards so the container matches content.
  int contentH = 0;
  int cardCount = 0;
  for (int i = 0; i < m_layout->count(); ++i) {
    QLayoutItem *item = m_layout->itemAt(i);
    if ((item == nullptr) || (item->widget() == nullptr)) {
      continue;
    }
    contentH += item->widget()->sizeHint().height();
    ++cardCount;
  }
  contentH += qMax(0, cardCount - 1) * m_layout->spacing();
  if (contentH > 0) {
    setFixedHeight(contentH);
  }
  emit sizeChanged();
}

void NotificationContainer::showEvent(QShowEvent *event) {
  setupLayerShell();
  QWidget::showEvent(event);
}

void NotificationContainer::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  emit sizeChanged();
}

void NotificationContainer::setupLayerShell() {
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
  shell->setExclusiveZone(-1); // no reserved space, floats over content
  shell->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
}

void NotificationContainer::paintEvent(QPaintEvent * /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QColor bg = m_cfg.background;

  // Draw every child card's rounded background + accent border. The cards are
  // transparent; their appearance is fully controlled here on the top-level
  // composited surface.
  for (int i = 0; i < m_layout->count(); ++i) {
    QLayoutItem *item = m_layout->itemAt(i);
    if ((item == nullptr) || (item->widget() == nullptr)) {
      continue;
    }
    auto *card = qobject_cast<NotificationCard *>(item->widget());
    if (card == nullptr) {
      continue;
    }
    const QRect r = card->geometry();
    if (r.isEmpty()) {
      continue;
    }
    QPainterPath path;
    path.addRoundedRect(r, m_cfg.radius, m_cfg.radius);

    QColor fill = bg;
    fill.setAlphaF(fill.alphaF() * m_cfg.backgroundOpacity);
    p.fillPath(path, fill);

    QColor border = card->style().accent;
    border.setAlpha(150);
    p.setPen(QPen(border, 1));
    p.drawPath(path);
  }
}