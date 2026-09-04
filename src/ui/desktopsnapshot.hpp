#pragma once

#include <QImage>
#include <QObject>

// Session-lifetime cache of one full-desktop screenshot, used as the source
// for real backdrop blur. Deliberately NOT auto-refreshed on a timer: the
// portal permission dialog would pop up repeatedly in the background, which
// is not acceptable for a notification daemon. Refresh happens once at
// startup and otherwise only on demand (`symm blur-refresh`).
class DesktopSnapshot : public QObject {
  Q_OBJECT
public:
  static DesktopSnapshot &instance();

  // Cached image, or null if none has been captured yet (or the portal
  // isn't available). Safe to call from paintEvent -- purely synchronous.
  QImage image() const { return m_image; }

  // Kicks off an async portal Screenshot request and updates the cache when
  // (if) it completes. Safe to call repeatedly; a request already in flight
  // is not duplicated.
  void refresh();

private:
  explicit DesktopSnapshot(QObject *parent = nullptr) : QObject(parent) {}

  QImage m_image;
  bool m_refreshing = false;
};
