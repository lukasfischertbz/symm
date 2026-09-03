#pragma once

#include <QColor>
#include <QPixmap>
#include <QRect>

// Best-effort, compositor-independent "kitty-style" backdrop blur.
//
// This tries to grab the desktop pixels behind `globalRect` and gaussian-blur
// them (real backdrop frost, like kitty's background_blur on X11/some
// Wayland setups). Most wlroots/Wayland compositors refuse arbitrary
// full-desktop capture for security, in which case the grab comes back
// black/empty; this is detected and we fall back to a synthesized
// frosted-glass panel (layered translucency + subtle noise) built from the
// notification's own theme color, so the result still looks intentional
// instead of flashing black or erroring.
QPixmap makeFrostedPanel(const QRect &globalRect, int blurRadius,
                         const QColor &tint);
