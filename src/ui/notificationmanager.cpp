#include "notificationmanager.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>

#include "../hyprland.hpp"
#include "notificationhistorywindow.hpp"
#include "notificationwindow.hpp"
#include "texture.hpp"

namespace {
// Resolves the QScreen matching the currently focused Hyprland output, or
// nullptr if that can't be determined (not Hyprland, hyprctl missing, no
// matching QScreen) -- callers should fall back to the default screen.
QScreen *resolveActiveMonitorScreen() {
  const MonitorInfo mon = activeHyprlandMonitor();
  if (!mon.valid) {
    return nullptr;
  }
  const QList<QScreen *> screens = QGuiApplication::screens();
  for (QScreen *s : screens) {
    if (s->geometry() == mon.geometry) {
      return s;
    }
  }
  return nullptr;
}
} // namespace

namespace {
QString historyFilePath() {
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
         QStringLiteral("/symm/history.json");
}
} // namespace

NotificationManager::NotificationManager(const Config &cfg, QObject *parent)
    : QObject(parent), m_cfg(cfg) {
  loadHistory();
}

void NotificationManager::show(const Notification &n) {
  m_cfg = Config::load();

  HistoryEntry entry;
  entry.id = n.id;
  entry.appName = n.appName;
  entry.summary = n.summary;
  entry.body = n.body;
  entry.urgency = n.urgency;
  entry.timestamp = QDateTime::currentDateTime();
  m_history.prepend(entry);
  trimHistory();
  saveHistory();

  // If we're already at the visible cap, queue it instead of creating a
  // window: this is also what makes the timeout "only start once visible"
  // -- the NotificationWindow (and its auto-dismiss QTimer) simply doesn't
  // exist yet for anything sitting in m_pending.
  if (m_cfg.maxVisible > 0 && m_windows.size() >= m_cfg.maxVisible) {
    m_pending.append(n);
    return;
  }

  displayNow(n);
}

void NotificationManager::displayNow(const Notification &n) {
  // Placement uses whichever monitor is focused right now; it does not
  // follow focus afterwards (a notification already on screen stays put).
  QScreen *targetScreen =
      m_cfg.useActiveMonitor ? resolveActiveMonitorScreen() : nullptr;

  // "Random icon" mode: pull a picture from a folder instead of whatever
  // (if anything) the sending app provided. Only the first decoded frame is
  // used even for an animated source file -- the icon label is a static
  // QLabel, not an animation target.
  Notification displayed = n;
  if (!m_cfg.iconSourceDir.isEmpty()) {
    const QString path = pickRandomTexture(m_cfg.iconSourceDir);
    if (!path.isEmpty()) {
      const QList<TextureFrame> frames = loadTextureFrames(path);
      if (!frames.isEmpty()) {
        displayed.icon = QIcon(QPixmap::fromImage(frames.first().image));
      }
    }
  }

  auto *win = new NotificationWindow(displayed, m_cfg, targetScreen, nullptr);
  m_windows.append(win);
  reflow();

  connect(win, &NotificationWindow::resized, this, [this] { reflow(); });

  // Remove the window on dismissal (click / timeout / explicit close) or when
  // an action button is invoked. Defer out of the emitting stack to avoid
  // re-entrant delete/close chains.
  connect(win, &NotificationWindow::dismissed, this, [this](uint id) {
    QTimer::singleShot(0, this, [this, id] { remove(id); });
  });
  connect(win, &NotificationWindow::actionInvoked, this,
          [this](uint id, const QString &key) {
            emit actionInvoked(id, key);
            QTimer::singleShot(0, this, [this, id] { remove(id); });
          });
  // Null out the slot if the window is destroyed outside remove(). When the
  // last window goes away there is nothing left to stack.
  connect(win, &QObject::destroyed, this, [this](QObject *obj) {
    for (QPointer<NotificationWindow> &p : m_windows) {
      if (p == obj) {
        p = nullptr;
      }
    }
  });
}

void NotificationManager::promoteFromQueue() {
  if (m_pending.isEmpty()) {
    return;
  }
  if (m_cfg.maxVisible > 0 && m_windows.size() >= m_cfg.maxVisible) {
    return;
  }
  const Notification next = m_pending.takeFirst();
  displayNow(next);
}

void NotificationManager::remove(uint id) {
  // Guard against re-entrant removal (e.g. a button's clicked handler chain
  // firing remove again for the same id before the first one unwinds).
  if (m_removing) {
    return;
  }
  m_removing = true;
  for (int i = 0; i < m_windows.size(); ++i) {
    NotificationWindow *win = m_windows.at(i);
    if ((win != nullptr) && win->id() == id) {
      // Disconnect so the window can't fire dismissed/actionInvoked into a
      // half-removed state.
      win->disconnect(this);
      win->close();
      win->deleteLater();
      m_windows.removeAt(i);
      break;
    }
  }
  reflow();
  m_removing = false;
  // A slot just freed up -- let the next queued notification (if any) in,
  // now that it's actually about to become visible (see displayNow's
  // comment on why its timer only starts here, not when it was received).
  promoteFromQueue();
}

void NotificationManager::showHistoryWindow() {
  if (m_historyWindow == nullptr) {
    m_historyWindow = new NotificationHistoryWindow(m_cfg, nullptr);
    connect(m_historyWindow, &QObject::destroyed, this,
            [this] { m_historyWindow = nullptr; });
  }
  m_historyWindow->setEntries(m_history);
  m_historyWindow->showTopRight();
}

void NotificationManager::reflow() {
  // Each monitor stacks independently: a card expanding or arriving on one
  // screen must never push notifications around on a different screen. (The
  // previous version tracked a single shared `top` counter for every window
  // regardless of which monitor it was on, which both mixed up multi-monitor
  // stacking and made expand-driven resizes cascade onto the wrong cards.)
  QHash<QScreen *, int> topByScreen;
  for (QPointer<NotificationWindow> &p : m_windows) {
    if (p == nullptr) {
      continue;
    }
    QScreen *screen = p->targetScreen();
    if (!topByScreen.contains(screen)) {
      topByScreen[screen] = m_cfg.top;
    }
    const int top = topByScreen[screen];
    p->setTopOffset(top);
    // sizeHint() reflects the card's *current* layout, so an expanded
    // ("Details") card correctly pushes the next one on the same monitor
    // further down.
    topByScreen[screen] = top + p->sizeHint().height() + m_cfg.cardSpacing;
  }
}

void NotificationManager::trimHistory() {
  const int max = qMax(0, m_cfg.historyMaxEntries);
  while (m_history.size() > max) {
    m_history.removeLast();
  }
}

void NotificationManager::loadHistory() {
  QFile file(historyFilePath());
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  file.close();
  if (!doc.isArray()) {
    return;
  }

  m_history.clear();
  for (const auto &val : doc.array()) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    HistoryEntry e;
    e.id = static_cast<uint>(obj.value(QStringLiteral("id")).toInt());
    e.appName = obj.value(QStringLiteral("appName")).toString();
    e.summary = obj.value(QStringLiteral("summary")).toString();
    e.body = obj.value(QStringLiteral("body")).toString();
    e.urgency = obj.value(QStringLiteral("urgency")).toInt();
    e.timestamp = QDateTime::fromString(
        obj.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
    m_history.append(e);
  }
}

void NotificationManager::saveHistory() const {
  QFileInfo info(historyFilePath());
  if (!info.dir().exists()) {
    info.dir().mkpath(QStringLiteral("."));
  }

  QFile file(historyFilePath());
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }

  QJsonArray array;
  for (const HistoryEntry &e : m_history) {
    QJsonObject obj;
    obj[QStringLiteral("id")] = static_cast<int>(e.id);
    obj[QStringLiteral("appName")] = e.appName;
    obj[QStringLiteral("summary")] = e.summary;
    obj[QStringLiteral("body")] = e.body;
    obj[QStringLiteral("urgency")] = e.urgency;
    obj[QStringLiteral("timestamp")] = e.timestamp.toString(Qt::ISODate);
    array.append(obj);
  }

  file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
  file.close();
}

void NotificationManager::clearHistory() {
  m_history.clear();
  saveHistory();
}