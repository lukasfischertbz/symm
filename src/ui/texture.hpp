#pragma once

#include <QImage>
#include <QList>
#include <QString>

// One decoded animation frame.
struct TextureFrame {
  QImage image;
  int delayMs = 100; // how long to hold this frame before the next
};

// Loads an image file as a sequence of frames for use as a card/icon/bar
// texture. Formats:
//   - PNG/JPEG/GIF/WEBP/BMP: decoded natively via QImageReader, including
//     GIF/animated-WEBP animation.
//   - AVIF: decoded via QImageReader IF your Qt install has the AVIF plugin
//     (qt6-imageformats on most distros); animated AVIF frames come through
//     the same way.
//   - JXL, EXR, TIFF, and anything else QImageReader can't open: Qt has no
//     built-in decoder for these. We shell out to `ffmpeg` (already on most
//     Hyprland/Linux setups) to convert it. ffmpeg only gives us a single
//     frame this way, so exotic formats render as a static texture rather
//     than animated -- there's no ffmpeg-based path to pull out every frame
//     of, say, an animated JXL without a lot more plumbing.
//
// Returns an empty list if the file doesn't exist or nothing could decode
// it.
QList<TextureFrame> loadTextureFrames(const QString &path);

// Picks a random image/animation file directly inside `dir` (non-recursive).
// Returns an empty string if the directory doesn't exist or has no files
// recognized by extension.
QString pickRandomTexture(const QString &dir);
