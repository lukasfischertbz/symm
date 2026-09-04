#pragma once

#include <QColor>
#include <QPixmap>
#include <QRect>

// Best-effort "kitty-style" backdrop blur for Wayland/X11.
//
// initBlurSource() grabs the whole screen once (via `grim` on Wayland,
// QScreen::grabWindow otherwise) and caches it. Grabbing the full desktop
// before any notification exists means the cached frame is guaranteed clean:
// re-grabbing per-card would either use stale invalid coordinates (the
// layer-shell surface is not positioned yet when showEvent fires) or capture
// the notification card itself (self-blur). Cards then crop their own rectangle
// out of this static snapshot.
//
// makeFrostedPanel() crops `globalRect` out of the cached snapshot and blurs
// it. When no real capture is available it falls back to a synthesized
// frosted-glass panel in `tint` so cards never render as flat/black.
void initBlurSource();
QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius,
                         const QColor &tint);