#include "blur.hpp"

#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QScreen>

namespace {

struct Snapshot {
  QImage image;
  // Global top-left corner of the captured region. grim's output pixel (0,0)
  // is the top-left of the geometry given to -g, which is not always global
  // (0,0) on multi-monitor setups with a left-of-primary display.
  QPoint origin;
};

Snapshot s_snapshot;

QPixmap gaussianBlur(const QPixmap &src, int radius) {
  if (src.isNull() || radius <= 0) {
    return src;
  }
  QGraphicsScene scene;
  auto *item = scene.addPixmap(src);
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

Snapshot fullScreenViaGrim() {
  QRect area;
  const QList<QScreen *> screens = QGuiApplication::screens();
  for (QScreen *s : screens) {
    area = area.united(s->geometry());
  }
  if (area.isEmpty()) {
    return {};
  }
  const QString geom = QStringLiteral("%1,%2,%3,%4")
                           .arg(area.x())
                           .arg(area.y())
                           .arg(area.width())
                           .arg(area.height());

  QProcess proc;
  proc.start(QStringLiteral("grim"),
             {QStringLiteral("-g"), geom, QStringLiteral("-t"),
              QStringLiteral("png"), QStringLiteral("-")});
  if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
    return {};
  }
  const QImage img = QImage::fromData(proc.readAllStandardOutput(), "PNG");
  if (img.isNull()) {
    return {};
  }
  return {img.copy(), area.topLeft()};
}

Snapshot fullScreenViaGrab() {
  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen == nullptr) {
    return {};
  }
  return {screen->grabWindow(0).toImage(), screen->geometry().topLeft()};
}

} // namespace

void initBlurSource() {
  s_snapshot = fullScreenViaGrim();
  if (s_snapshot.image.isNull()) {
    s_snapshot = fullScreenViaGrab();
  }
}

QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius) {
  if (globalRect.isEmpty() || s_snapshot.image.isNull()) {
    return {};
  }

  // Translate the card's global rect into the snapshot's local coordinate
  // space, then clamp to its bounds.
  const QRect clip = globalRect.translated(-s_snapshot.origin)
                         .intersected(s_snapshot.image.rect());
  if (clip.isEmpty()) {
    return {};
  }

  const QPixmap raw = QPixmap::fromImage(s_snapshot.image.copy(clip));
  if (raw.isNull()) {
    return {};
  }

  // The returned pixmap is exactly clip.size(), so the card paints it at
  // (0,0) -- pixel-perfect under the card regardless of screen arrangement.
  return gaussianBlur(raw, blurRadius);
}