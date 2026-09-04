#include "desktopsnapshot.hpp"

#include "portal_screenshot.hpp"

DesktopSnapshot &DesktopSnapshot::instance() {
  static DesktopSnapshot inst;
  return inst;
}

void DesktopSnapshot::refresh() {
  if (m_refreshing) {
    return;
  }
  m_refreshing = true;
  requestPortalScreenshot([this](const QImage &img) {
    m_refreshing = false;
    if (!img.isNull()) {
      m_image = img;
    }
  });
}
