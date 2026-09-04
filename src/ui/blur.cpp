#include "blur.hpp"

#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QProcess>
#include <QScreen>

namespace {

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

QPixmap captureViaGrim(const QRect &globalRect) {
  const QString geom = QStringLiteral("%1,%2,%3,%4")
                           .arg(globalRect.x())
                           .arg(globalRect.y())
                           .arg(globalRect.width())
                           .arg(globalRect.height());

  QProcess proc;
  proc.start(QStringLiteral("grim"),
             {QStringLiteral("-g"), geom, QStringLiteral("-t"),
              QStringLiteral("png"), QStringLiteral("-")});
  if (!proc.waitForFinished(2000) || proc.exitCode() != 0) {
    return {};
  }
  QPixmap px;
  px.loadFromData(proc.readAllStandardOutput(), "PNG");
  return px;
}

QPixmap captureViaGrabWindow(const QRect &globalRect) {
  if (QScreen *screen = QApplication::primaryScreen()) {
    return screen->grabWindow(0, globalRect.x(), globalRect.y(),
                              globalRect.width(), globalRect.height());
  }
  return {};
}

} // namespace

QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius) {
  if (globalRect.isEmpty()) {
    return {};
  }

  QPixmap raw = captureViaGrim(globalRect);
  if (raw.isNull()) {
    raw = captureViaGrabWindow(globalRect);
  }
  if (raw.isNull()) {
    return {};
  }

  return gaussianBlur(raw, blurRadius);
}
