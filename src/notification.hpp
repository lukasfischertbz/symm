#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

struct Notification {
  uint id = 0;
  QString appName;
  QString summary;
  QString body;
  QIcon icon;
  int timeoutMs = 10000; // -1 = permanent
  int urgency = 1;       // 0 low, 1 normal, 2 critical
  QStringList actions;
  bool persist = false; // stays until clicked (no auto-timeout)
};
