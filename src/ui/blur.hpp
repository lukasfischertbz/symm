#pragma once

#include <QPixmap>
#include <QRect>

// Best-effort "kitty-style" backdrop blur. Prefers a native Wayland grab via
// `grim` (Hyprland/slurp tooling); falls back to QScreen::grabWindow when grim
// is unavailable (X11). Returns a NULL pixmap if every capture path fails --
// callers just draw the flat tinted fill instead of blur.
QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius);
