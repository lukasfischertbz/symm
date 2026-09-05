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
// real desktop pixels behind the notification (the common Wayland case).
//
// Instead of a flat gradient + speckle (which reads as a solid panel, not
// frosted glass), generate a noisy backdrop and gaussian-blur it -- the same
// blur primitive used for real captures -- so the panel actually looks like
// blurred content behind translucent glass, in the theme color rather than
// flat black. `tint` is used only to warm/cool the noise, not at full
// opacity, so even a navy tint won't wash the card blue.
QPixmap synthesizedFrost(const QSize &size, const QColor &tint) {
  QPixmap base(size);
  base.fill(Qt::transparent);
  {
    QPainter p(&base);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    // Per-pixel brightness noise so the gaussian below has real texture to
    // blur. Spread the luminance around the tint's own gray level: bright and
    // dark splotches, never a flat wash.
    QRandomGenerator *rng = QRandomGenerator::global();
    const int step = 3;
    for (int y = 0; y < size.height(); y += step) {
      for (int x = 0; x < size.width(); x += step) {
        QColor c = tint;
        const int jitter = rng->bounded(-60, 61);
        c = c.lighter(qBound(50, 100 + jitter / 3, 190));
        const int a = rng->bounded(40, 130);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), a));
        const int w = rng->bounded(3, 12);
        p.drawEllipse(QPointF(x + step / 2.0, y + step / 2.0), w, w);
      }
    }
  }

  // Real gaussian blur of the noise so it reads as frosted glass (kitty-style)
  // instead of noise specks, then composite over an opaque neutral base so
  // nothing behind shows through.
  const int radius = qBound(6, (size.width() + size.height()) / 40, 40);
  QPixmap blurred = gaussianBlur(base, radius);

  QPixmap out(size);
  out.fill(Qt::transparent);
  {
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    // Deep, near-neutral underlay so the frosted surface has body and the
    // panel never reads as a bright/blue rectangle.
    QColor bed = tint.lighter(112);
    bed.setAlpha(255);
    p.fillRect(out.rect(), bed);
    p.drawPixmap(0, 0, blurred);
  }
  return out;
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

bool runningOnHyprland() {
  return !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE");
}

bool enableCompositorBlur() {
  if (!runningOnHyprland()) {
    return false;
  }
  // layerrule is per keyword value: `keyword layerrule RULE`. The rules match
  // surfaces whose layer-shell namespace is "notifier" (setScope in the card
  // windows). `blur` asks Hyprland to blur the live framebuffer behind the
  // surface each frame (kitty's exact mechanism); `ignorealpha` keeps fully
  // transparent pixels -- the card's rounded corners -- out of the blur so it
  // follows the rounded-rect shape instead of a hard rectangle.
  const auto runRule = [](const QString &rule) {
    QProcess proc;
    proc.start(QStringLiteral("hyprctl"),
               {QStringLiteral("keyword"), QStringLiteral("layerrule"), rule});
    return proc.waitForFinished(1000) &&
           proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
  };
  const bool blur = runRule(QStringLiteral("blur,notifier"));
  runRule(QStringLiteral("ignorealpha,notifier"));
  return blur;
}

QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius,
                         const QColor &tint) {
  if (globalRect.isEmpty()) {
    return {};
  }

  // Real desktop pixels: crop a region of the cached snapshot and blur it.
  // The sample region is expanded well beyond the card's own bounds so the
  // blur bleeds continuously across the whole card -- kitty-style -- instead
  // of being computed on a tiny isolated patch (which reads as a hard-edged
  // sticker). The blurred result is then cropped back to the card size.
  if (!s_snapshot.image.isNull()) {
    const int bleed = qMax(64, blurRadius * 4);
    const QRect wide = globalRect.adjusted(-bleed, -bleed, bleed, bleed);
    const QRect clip = wide.translated(-s_snapshot.origin)
                           .intersected(s_snapshot.image.rect());
    if (!clip.isEmpty()) {
      const QPixmap raw = QPixmap::fromImage(s_snapshot.image.copy(clip));
      if (!raw.isNull()) {
        const QPixmap blurred = gaussianBlur(raw, blurRadius);
        // The blurred image is in snapshot-local coords sized to `clip`; the
        // card sits at its own snapshot-local position inside it. Crop that
        // back out (the bleed ensures the blur is continuous across the card).
        const QRect cardLocal = globalRect.translated(-s_snapshot.origin);
        const QRect crop(cardLocal.left() - clip.left(),
                         cardLocal.top() - clip.top(), globalRect.width(),
                         globalRect.height());
        return blurred.copy(crop.intersected(blurred.rect()));
      }
    }
  }

  // No usable capture (grim absent, grab refused black, early boot): fall
  // back to a synthesized frosted-glass panel built from the theme color so
  // cards keep a glassy look instead of turning into flat/black rectangles.
  return synthesizedFrost(globalRect.size(), tint);
}