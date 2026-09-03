#include "blur.hpp"

#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>

namespace {

// Heuristic: a real screen grab has pixel variance; a grab that was silently
// refused by the compositor comes back as a single flat color (usually
// black). Sample a handful of points instead of the whole image for speed.
bool looksLikeRealCapture(const QImage &img) {
  if (img.isNull()) {
    return false;
  }
  const int w = img.width();
  const int h = img.height();
  if (w < 2 || h < 2) {
    return false;
  }
  const QRgb first = img.pixel(0, 0);
  for (int i = 1; i < 8; ++i) {
    const int x = (w * i) / 8;
    const int y = (h * i) / 8;
    if (img.pixel(x, y) != first) {
      return true;
    }
  }
  return false;
}

QPixmap gaussianBlur(const QPixmap &src, int radius) {
  if (src.isNull() || radius <= 0) {
    return src;
  }
  QGraphicsScene scene;
  QGraphicsPixmapItem *item = scene.addPixmap(src);
  auto *effect = new QGraphicsBlurEffect;
  effect->setBlurRadius(radius);
  effect->setBlurHints(QGraphicsBlurEffect::QualityHint);
  item->setGraphicsEffect(effect);

  QPixmap out(src.size());
  out.fill(Qt::transparent);
  QPainter p(&out);
  p.setRenderHint(QPainter::Antialiasing);
  scene.render(&p, QRectF(0, 0, src.width(), src.height()),
               QRectF(0, 0, src.width(), src.height()));
  return out;
}

// Fabricated frosted-glass look used whenever we can't legitimately read the
// real desktop pixels behind the notification (the common Wayland case).
QPixmap synthesizedFrost(const QSize &size, const QColor &tint) {
  QPixmap pm(size);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);

  QLinearGradient grad(0, 0, 0, size.height());
  QColor top = tint.lighter(120);
  QColor bottom = tint.darker(115);
  top.setAlpha(255);
  bottom.setAlpha(255);
  grad.setColorAt(0.0, top);
  grad.setColorAt(1.0, bottom);
  p.fillRect(pm.rect(), grad);

  // Faint speckle so it reads as "glass" rather than a flat gradient.
  p.setPen(Qt::NoPen);
  QRandomGenerator *rng = QRandomGenerator::global();
  const int speckleCount = (size.width() * size.height()) / 900;
  for (int i = 0; i < speckleCount; ++i) {
    const int x = rng->bounded(size.width());
    const int y = rng->bounded(size.height());
    const int a = rng->bounded(8, 20);
    p.setBrush(QColor(255, 255, 255, a));
    p.drawEllipse(QPointF(x, y), 1.0, 1.0);
  }
  return pm;
}

} // namespace

QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius,
                         const QColor &tint) {
  QScreen *screen = QApplication::primaryScreen();
  QPixmap raw;
  if (screen != nullptr && !globalRect.isEmpty()) {
    // WId 0 == grab from the root/virtual desktop. On X11 (and Wayland
    // compositors that permit it) this returns real pixels; elsewhere it
    // typically comes back black, caught by looksLikeRealCapture() below.
    raw = screen->grabWindow(0, globalRect.x(), globalRect.y(),
                             globalRect.width(), globalRect.height());
  }

  if (!raw.isNull() && looksLikeRealCapture(raw.toImage())) {
    return gaussianBlur(raw, blurRadius);
  }
  return synthesizedFrost(globalRect.size(), tint);
}
