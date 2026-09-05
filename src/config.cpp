#include "config.hpp"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QMap>
#include <QStandardPaths>

namespace {

// A simple INI-style reader: "[section]" headers and "key = value" lines.
// Comments start with '#' or ';'. Keys are unique per section.
struct IniData {
  QMap<QString, QMap<QString, QString>> sections;

  bool parse(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return false;
    }

    QString current;
    while (!f.atEnd()) {
      const QString raw = QString::fromUtf8(f.readLine()).trimmed();
      if (raw.isEmpty() || raw.startsWith(QLatin1Char('#')) ||
          raw.startsWith(QLatin1Char(';'))) {
        continue;
      }
      if (raw.startsWith(QLatin1Char('[')) && raw.endsWith(QLatin1Char(']'))) {
        current = raw.mid(1, raw.size() - 2).trimmed();
        continue;
      }
      const int eq = static_cast<int>(raw.indexOf(QLatin1Char('=')));
      if (eq < 0) {
        continue;
      }
      const QString key = raw.left(eq).trimmed();
      QString val = raw.mid(eq + 1).trimmed();
      if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) &&
          val.size() >= 2) {
        val = val.mid(1, val.size() - 2);
      }
      sections[current][key] = val;
    }
    return true;
  }

  QString value(const QString &section, const QString &key,
                const QString &fallback = QString()) const {
    const auto it = sections.constFind(section);
    if (it != sections.constEnd()) {
      const auto it2 = it->constFind(key);
      if (it2 != it->constEnd()) {
        return it2.value();
      }
    }
    return fallback;
  }

  // Layers another INI over this one per key: keys defined in `o` replace
  // ours; anything `o` omits keeps our value. Later layers win.
  void merge(const IniData &o) {
    for (auto it = o.sections.cbegin(); it != o.sections.cend(); ++it) {
      for (auto it2 = it.value().cbegin(); it2 != it.value().cend(); ++it2) {
        sections[it.key()][it2.key()] = it2.value();
      }
    }
  }
};

} // namespace

QString Config::urgencyColorKey(int urgency) {
  switch (urgency) {
  case 0:
    return QStringLiteral("low");
  case 2:
    return QStringLiteral("critical");
  default:
    return QStringLiteral("normal");
  }
}

Config Config::load() {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
      QStringLiteral("/symm");

  // Layered config, highest precedence first:
  //   symm.user.ini > symm.theme.ini > symm.sys.ini > symm.ini
  // Missing layers are skipped.
  IniData ini;
  const QString basePath = QDir(dir).filePath(QStringLiteral("symm.ini"));
  ini.parse(basePath);

  IniData layer;
  if (layer.parse(QDir(dir).filePath(QStringLiteral("symm.sys.ini")))) {
    ini.merge(layer);
  }
  if (layer.parse(QDir(dir).filePath(QStringLiteral("symm.theme.ini")))) {
    ini.merge(layer);
  }
  if (layer.parse(QDir(dir).filePath(QStringLiteral("symm.user.ini")))) {
    ini.merge(layer);
  }

  Config c;

  auto readInt = [&](const QString &key, int fallback) {
    bool ok = false;
    const int v = ini.value(QStringLiteral("general"), key).toInt(&ok);
    return ok ? v : fallback;
  };

  c.width = readInt(QStringLiteral("width"), c.width);
  c.margin = readInt(QStringLiteral("margin"), c.margin);
  c.top = readInt(QStringLiteral("top"), c.top);
  c.radius = readInt(QStringLiteral("radius"), c.radius);
  c.paddingH = readInt(QStringLiteral("padding_h"), c.paddingH);
  c.paddingV = readInt(QStringLiteral("padding_v"), c.paddingV);
  c.gap = readInt(QStringLiteral("gap"), c.gap);
  c.cardSpacing = readInt(QStringLiteral("card_spacing"), c.cardSpacing);
  c.timeoutDefaultMs =
      readInt(QStringLiteral("timeout_low"), c.timeoutDefaultMs);
  c.timeoutNormalMs =
      readInt(QStringLiteral("timeout_normal"), c.timeoutNormalMs);
  c.timeoutCriticalMs =
      readInt(QStringLiteral("timeout_critical"), c.timeoutCriticalMs);
  c.historyMaxEntries =
      readInt(QStringLiteral("history_max_entries"), c.historyMaxEntries);
  c.historyRecentCount =
      readInt(QStringLiteral("history_recent_count"), c.historyRecentCount);

  auto readBool = [&](const QString &key, bool fallback) {
    const QString v = ini.value(QStringLiteral("general"), key);
    if (v.isEmpty()) {
      return fallback;
    }
    return v == QStringLiteral("true") || v == QStringLiteral("1");
  };

  c.iconsEnabled = readBool(QStringLiteral("icons_enabled"), c.iconsEnabled);
  c.iconSize = readInt(QStringLiteral("icon_size"), c.iconSize);
  c.bodyTruncateChars =
      readInt(QStringLiteral("body_truncate_chars"), c.bodyTruncateChars);
  c.blurEnabled = readBool(QStringLiteral("blur_enabled"), c.blurEnabled);
  c.blurRadius = readInt(QStringLiteral("blur_radius"), c.blurRadius);
  c.useActiveMonitor =
      readBool(QStringLiteral("use_active_monitor"), c.useActiveMonitor);

  c.backgroundImage =
      ini.value(QStringLiteral("general"), QStringLiteral("background_image"));
  c.barImage =
      ini.value(QStringLiteral("general"), QStringLiteral("bar_image"));
  c.iconSourceDir =
      ini.value(QStringLiteral("general"), QStringLiteral("icon_source_dir"));

  c.maxVisible = readInt(QStringLiteral("max_visible"), c.maxVisible);

  auto readStr = [&](const QString &key, const QString &fallback) {
    const QString v = ini.value(QStringLiteral("general"), key);
    return v.isEmpty() ? fallback : v;
  };
  c.actionButtonStyle =
      readStr(QStringLiteral("action_button_style"), c.actionButtonStyle);
  c.actionButtonPosition =
      readStr(QStringLiteral("action_button_position"), c.actionButtonPosition);
  c.barStyle = readStr(QStringLiteral("bar_style"), c.barStyle);
  c.barPosition = readStr(QStringLiteral("bar_position"), c.barPosition);
  c.barFill = readBool(QStringLiteral("bar_fill"), c.barFill);
  c.barMoveRight = readBool(QStringLiteral("bar_move_right"), c.barMoveRight);
  c.backgroundImageAnchored = readBool(
      QStringLiteral("background_image_anchored"), c.backgroundImageAnchored);

  {
    bool ok = false;
    const double v = ini.value(QStringLiteral("general"),
                               QStringLiteral("background_opacity"))
                         .toDouble(&ok);
    if (ok) {
      c.backgroundOpacity = std::clamp(v, 0.0, 1.0);
    }
  }

  {
    bool ok = false;
    const double v =
        ini.value(QStringLiteral("general"), QStringLiteral("font_size"))
            .toDouble(&ok);
    if (ok) {
      c.fontSize = v;
    }
  }

  const QString family =
      ini.value(QStringLiteral("general"), QStringLiteral("font_family"));
  if (!family.isEmpty()) {
    c.fontFamily = family;
  }

  const QString bg =
      ini.value(QStringLiteral("colors"), QStringLiteral("background"));
  if (!bg.isEmpty()) {
    c.background = QColor(bg);
  }
  const QString fg =
      ini.value(QStringLiteral("colors"), QStringLiteral("foreground"));
  if (!fg.isEmpty()) {
    c.textColor = QColor(fg);
  }
  const QString dim =
      ini.value(QStringLiteral("colors"), QStringLiteral("dim_foreground"));
  if (!dim.isEmpty()) {
    c.dimTextColor = QColor(dim);
  }

  auto readStyle = [&ini](const QString &section,
                          const UrgencyStyle &fallback) {
    UrgencyStyle out = fallback;
    const QString bar = ini.value(section, QStringLiteral("bar"));
    const QString accent = ini.value(section, QStringLiteral("accent"));
    if (!bar.isEmpty()) {
      out.bar = QColor(bar);
    }
    if (!accent.isEmpty()) {
      out.accent = QColor(accent);
    }
    return out;
  };

  c.low = readStyle(QStringLiteral("urgent_low"), c.low);
  c.normal = readStyle(QStringLiteral("urgent_normal"), c.normal);
  c.warning = readStyle(QStringLiteral("urgent_warning"), c.warning);
  c.error = readStyle(QStringLiteral("urgent_error"), c.error);
  c.critical = readStyle(QStringLiteral("urgent_critical"), c.critical);

  return c;
}
