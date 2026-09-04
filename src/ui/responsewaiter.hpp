#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <functional>

#include "portal_screenshot.hpp"

// Receives the portal's async Response signal for one in-flight request,
// then deletes itself.
class ResponseWaiter : public QObject {
  Q_OBJECT
public:
  explicit ResponseWaiter(ScreenshotCallback cb) : m_callback(std::move(cb)) {}

public slots:
  void onResponse(uint code, const QVariantMap &results) {
    QImage img;
    if (code == 0) {
      const QString uri = results.value(QStringLiteral("uri")).toString();
      const QString path = QUrl(uri).toLocalFile();
      if (!path.isEmpty()) {
        img.load(path);
      }
    }
    m_callback(img);
    deleteLater();
  }

private:
  ScreenshotCallback m_callback;
};
