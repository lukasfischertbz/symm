#include "notificationmanager.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

#include "notificationcard.hpp"
#include "notificationcontainer.hpp"
#include "notificationhistorywindow.hpp"

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
  {
    QFile dbg(QStringLiteral("/home/hitler/source/notifier/build/cfg.txt"));
    if (dbg.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      dbg.write((QStringLiteral("paddingH=%1 paddingV=%2 cardSpacing=%3\n")
                     .arg(m_cfg.paddingH)
                     .arg(m_cfg.paddingV)
                     .arg(m_cfg.cardSpacing))
                    .toUtf8());
    }
  }

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

  if (m_container == nullptr) {
    m_container = new NotificationContainer(m_cfg, nullptr);
  }

  auto *card = new NotificationCard(n, m_cfg, m_container);
  m_cards.append(card);
  // Size the container to hold (all) cards, then (re)show it so the layer
  // surface is mapped at the correct extent. Mapping before sizing makes the
  // compositor shrink the surface to the default widget size.
  m_container->addCard(card);
  if (!m_container->isVisible()) {
    m_container->show();
  }

  // Remove the card + reflow (the layout does the reflow automatically) on
  // dismissal by click, timeout, or explicit close.
  connect(card, &NotificationCard::dismissed, this, [this](uint id) {
    QTimer::singleShot(0, this, [this, id] { remove(id); });
  });
  // Forward an invoked action up to the server so it can emit ActionInvoked.
  // Defer removal out of the button's clicked-handler stack (which is still
  // unwinding) to avoid re-entrant delete/close inside the D-Bus emission.
  connect(card, &NotificationCard::actionInvoked, this,
          [this](uint id, const QString &key) {
            emit actionInvoked(id, key);
            QTimer::singleShot(0, this, [this, id] { remove(id); });
          });
  // If the card is destroyed by any path outside remove(), null its slot so
  // m_cards never holds a stale pointer. Guard the container so a destroyed
  // container can never be dereferenced.
  connect(card, &QObject::destroyed, this, [this](QObject *obj) {
    for (QPointer<NotificationCard> &p : m_cards) {
      if (p == obj) {
        p = nullptr;
      }
    }
    if (m_cards.isEmpty() && m_container) {
      m_container->close();
    }
  });
}

void NotificationManager::remove(uint id) {
  // Guard against re-entrant removal (e.g. a button's clicked handler chain
  // firing remove again for the same id before the first one unwinds).
  if (m_removing) {
    return;
  }
  m_removing = true;
  for (int i = 0; i < m_cards.size(); ++i) {
    NotificationCard *card = m_cards.at(i);
    if ((card != nullptr) && card->id() == id) {
      // Disconnect so the card can't fire dismissed/actionInvoked into a
      // half-removed state.
      card->disconnect(this);
      if (m_container) {
        m_container->removeCard(card);
      }
      m_cards.removeAt(i);
      break;
    }
  }
  if (m_cards.isEmpty() && (m_container != nullptr)) {
    m_container->close();
  }
  m_removing = false;
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
  for (const QJsonValue &val : doc.array()) {
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