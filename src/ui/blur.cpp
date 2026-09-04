#include "blur.hpp"

#include <QColor>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGuiApplication>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QProcess>
#include <QRandomGenerator>
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

// A failed capture can still be a non-null image: grim may run before the
// compositor painted anything, and QScreen::grabWindow() falls back to a solid
// black frame on Wayland. Accept only a frame that actually looks like a
// desktop -- real brightness and variance. When there's no usable frame,
// makeFrostedPanel() reroutes to synthesizedFrost() so cards still look like
// frosted glass instead of black rectangles.
bool looksLikeRealCapture(const QImage &img) {
  if (img.isNull() || img.width() < 8 || img.height() < 8) {
    return false;
  }
  const QImage rgb = img.convertToFormat(QImage::Format_RGB32);
  const int step = qMax(1, qMax(rgb.width(), rgb.height()) / 32);
  double sum = 0.0;
  double sumSq = 0.0;
  long count = 0;
  for (int y = 0; y < rgb.height(); y += step) {
    const QRgb *line = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
    for (int x = 0; x < rgb.width(); x += step) {
      const QRgb px = line[x];
      const double lum =
          0.2126 * qRed(px) + 0.7152 * qGreen(px) + 0.0722 * qBlue(px);
      sum += lum;
      sumSq += lum * lum;
      ++count;
    }
  }
  if (count == 0) {
    return false;
  }
  const double mean = sum / static_cast<double>(count);
  const double variance =
      qMax(0.0, sumSq / static_cast<double>(count) - mean * mean);
  return mean > 12.0 && variance > 8.0;
}

// Fabricated frosted-glass look used whenever we can't legitimately read the
// real desktop pixels behind the notification (the common Wayland case):
// layered translucency in the theme color plus faint noise, so cards still
// look like intentional frosted glass instead of flat black.
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

void initBlurSource() {
  s_snapshot = fullScreenViaGrim();
  if (!looksLikeRealCapture(s_snapshot.image)) {
    s_snapshot = fullScreenViaGrab();
  }
  if (!looksLikeRealCapture(s_snapshot.image)) {
    s_snapshot = Snapshot{};
  }
}

QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius,
                         const QColor &tint) {
  if (globalRect.isEmpty()) {
    return {};
  }

  // Real desktop pixels: crop the cached snapshot when there's a valid one.
  if (!s_snapshot.image.isNull()) {
    // Translate the card's global rect into the snapshot's local coordinate
    // space, then clamp to its bounds.
    const QRect clip = globalRect.translated(-s_snapshot.origin)
                           .intersected(s_snapshot.image.rect());
    if (!clip.isEmpty()) {
      const QPixmap raw = QPixmap::fromImage(s_snapshot.image.copy(clip));
      if (!raw.isNull()) {
        return gaussianBlur(raw, blurRadius);
      }
    }
  }

  // No usable capture (grim absent, grab refused black, early boot): fall
  // back to a synthesized frosted-glass panel built from the theme color so
  // cards keep a glassy look instead of turning into flat/black rectangles.
  return synthesizedFrost(globalRect.size(), tint);
}