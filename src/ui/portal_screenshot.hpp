#pragma once

#include <QImage>
#include <functional>

using ScreenshotCallback = std::function<void(QImage)>;

// Requests ONE full-desktop screenshot via xdg-desktop-portal
// (org.freedesktop.portal.Screenshot). This is the only way to legitimately
// read real desktop pixels under Wayland, including Hyprland -- but most
// portal implementations show a permission/preview dialog EVERY time this is
// called, with no "remember my choice" option (unlike the ScreenCast
// portal). That makes it unsuitable to call repeatedly in the background;
// callers should invoke this once (e.g. at daemon startup, or on an explicit
// `symm blur-refresh`) and cache the result -- see DesktopSnapshot.
//
// Delivered asynchronously. `callback` receives a null QImage on failure,
// denial, or if no portal implementation is running (e.g. no
// xdg-desktop-portal-hyprland/wlr installed).
void requestPortalScreenshot(ScreenshotCallback callback);
