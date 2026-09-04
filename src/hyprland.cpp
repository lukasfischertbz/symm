#include "hyprland.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

MonitorInfo activeHyprlandMonitor() {
  MonitorInfo info;

  QProcess proc;
  proc.start(QStringLiteral("hyprctl"),
             {QStringLiteral("-j"), QStringLiteral("monitors")});
  if (!proc.waitForFinished(300)) {
    proc.kill();
    return info; // hyprctl missing, hanging, or not on Hyprland.
  }
  if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
    return info;
  }

  const QJsonDocument doc =
      QJsonDocument::fromJson(proc.readAllStandardOutput());
  if (!doc.isArray()) {
    return info;
  }

  for (const auto &val : doc.array()) {
    const QJsonObject o = val.toObject();
    if (!o.value(QStringLiteral("focused")).toBool()) {
      continue;
    }
    const int x = o.value(QStringLiteral("x")).toInt();
    const int y = o.value(QStringLiteral("y")).toInt();
    const int wPx = o.value(QStringLiteral("width")).toInt();
    const int hPx = o.value(QStringLiteral("height")).toInt();
    const double scale = o.value(QStringLiteral("scale")).toDouble(1.0);
    // hyprctl reports physical pixel dimensions; QScreen::geometry() is in
    // logical (scaled) coordinates, so convert to match.
    const int wLogical = scale > 0 ? static_cast<int>(wPx / scale) : wPx;
    const int hLogical = scale > 0 ? static_cast<int>(hPx / scale) : hPx;
    info.geometry = QRect(x, y, wLogical, hLogical);
    info.valid = true;
    break;
  }
  return info;
}
