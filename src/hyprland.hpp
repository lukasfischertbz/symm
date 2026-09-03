#pragma once

#include <QRect>

// Info about the currently focused Hyprland output, in logical (scaled)
// desktop coordinates matching QScreen::geometry().
struct MonitorInfo {
  QRect geometry;
  bool valid = false;
};

// Asks `hyprctl -j monitors` for the monitor with "focused": true. Returns
// valid=false if hyprctl isn't on PATH, times out, or we're not running under
// Hyprland at all (e.g. any other compositor) -- callers should fall back to
// the default/primary screen in that case.
MonitorInfo activeHyprlandMonitor();
