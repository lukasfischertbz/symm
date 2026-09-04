#include "texture.hpp"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRandomGenerator>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

namespace {

// Extensions QImageReader has no chance with out of the box; route these
// straight to the ffmpeg fallback instead of wasting a failed decode.
bool needsFfmpegFallback(const QString &suffix) {
  static const QSet<QString> exotic = {
      QStringLiteral("jxl"), QStringLiteral("exr"), QStringLiteral("tiff"),
      QStringLiteral("tif")};
  return exotic.contains(suffix.toLower());
}

QList<TextureFrame> loadViaQImageReader(const QString &path) {
  QList<TextureFrame> frames;
  QImageReader reader(path);
  reader.setAutoTransform(true);
  if (!reader.canRead()) {
    return frames;
  }
  const int frameCount = qMax(1, reader.imageCount());
  for (int i = 0; i < frameCount; ++i) {
    QImage img = reader.read();
    if (img.isNull()) {
      break;
    }
    TextureFrame f;
    f.image = img;
    f.delayMs = reader.nextImageDelay() > 0 ? reader.nextImageDelay() : 100;
    frames.append(f);
    if (!reader.jumpToNextImage()) {
      break;
    }
  }
  return frames;
}

// Single-frame decode via ffmpeg for formats Qt can't open natively
// (JXL, EXR, TIFF, and anything else that falls through above).
QList<TextureFrame> loadViaFfmpeg(const QString &path) {
  QList<TextureFrame> frames;
  QTemporaryDir tmp;
  if (!tmp.isValid()) {
    return frames;
  }
  const QString outPath =
      tmp.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) +
                   QStringLiteral(".png"));

  QProcess proc;
  proc.start(QStringLiteral("ffmpeg"),
             {QStringLiteral("-y"), QStringLiteral("-loglevel"),
              QStringLiteral("error"), QStringLiteral("-i"), path,
              QStringLiteral("-frames:v"), QStringLiteral("1"), outPath});
  if (!proc.waitForFinished(4000)) {
    proc.kill();
    return frames;
  }
  if (proc.exitCode() != 0) {
    return frames; // ffmpeg missing, or genuinely can't decode this file
  }

  QImage img(outPath);
  if (!img.isNull()) {
    frames.append(TextureFrame{img, 0});
  }
  return frames;
}

} // namespace

QList<TextureFrame> loadTextureFrames(const QString &path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return {};
  }
  const QString suffix = QFileInfo(path).suffix();
  if (!needsFfmpegFallback(suffix)) {
    QList<TextureFrame> frames = loadViaQImageReader(path);
    if (!frames.isEmpty()) {
      return frames;
    }
  }
  return loadViaFfmpeg(path);
}

QString pickRandomTexture(const QString &dir) {
  QDir d(dir);
  if (!d.exists()) {
    return {};
  }
  static const QStringList filters = {
      QStringLiteral("*.png"),  QStringLiteral("*.jpg"),
      QStringLiteral("*.jpeg"), QStringLiteral("*.gif"),
      QStringLiteral("*.webp"), QStringLiteral("*.bmp"),
      QStringLiteral("*.avif"), QStringLiteral("*.jxl"),
      QStringLiteral("*.tiff"), QStringLiteral("*.tif"),
      QStringLiteral("*.exr")};
  const QStringList entries =
      d.entryList(filters, QDir::Files | QDir::Readable);
  if (entries.isEmpty()) {
    return {};
  }
  const int idx =
      QRandomGenerator::global()->bounded(static_cast<int>(entries.size()));
  return d.filePath(entries.at(idx));
}
