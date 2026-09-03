#include "config.hpp"

#include <QDir>
#include <QFile>
#include <QMap>
#include <QStandardPaths>
#include <QStringList>
#include <QDebug>

namespace {

// A simple INI-style reader: "[section]" headers and "key = value" lines.
// Comments start with '#' or ';'. Keys are unique per section.
struct IniData {
    QMap<QString, QMap<QString, QString>> sections;

    bool parse(const QString& path) {
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
            const int eq = raw.indexOf(QLatin1Char('='));
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

    QString value(const QString& section, const QString& key,
                  const QString& fallback = QString()) const {
        const auto it = sections.constFind(section);
        if (it != sections.constEnd()) {
            const auto it2 = it->constFind(key);
            if (it2 != it->constEnd()) {
                return it2.value();
            }
        }
        return fallback;
    }
};

} // namespace

QString Config::urgencyColorKey(int urgency) const {
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
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString path = QDir(dir).filePath(QStringLiteral("symm/config.conf"));

    IniData ini;
    if (!ini.parse(path)) {
        return Config{};
    }

    Config c;

    auto readInt = [&](const QString& key, int fallback) {
        bool ok = false;
        const int v = ini.value(QStringLiteral("general"), key).toInt(&ok);
        return ok ? v : fallback;
    };

    c.width = readInt(QStringLiteral("width"), c.width);
    c.margin = readInt(QStringLiteral("margin"), c.margin);
    c.top = readInt(QStringLiteral("top"), c.top);
    c.radius = readInt(QStringLiteral("radius"), c.radius);
    c.timeoutDefaultMs = readInt(QStringLiteral("timeout_low"), c.timeoutDefaultMs);
    c.timeoutNormalMs = readInt(QStringLiteral("timeout_normal"), c.timeoutNormalMs);
    c.timeoutCriticalMs = readInt(QStringLiteral("timeout_critical"), c.timeoutCriticalMs);
    c.timerDefaultMs = readInt(QStringLiteral("timer_default"), c.timerDefaultMs);
    c.historyMaxEntries = readInt(QStringLiteral("history_max_entries"), c.historyMaxEntries);

    {
        bool ok = false;
        const double v = ini.value(QStringLiteral("general"), QStringLiteral("font_size")).toDouble(&ok);
        if (ok) c.fontSize = v;
    }

    const QString family =
        ini.value(QStringLiteral("general"), QStringLiteral("font_family"));
    if (!family.isEmpty()) {
        c.fontFamily = family;
    }

    const QString bg = ini.value(QStringLiteral("colors"), QStringLiteral("background"));
    if (!bg.isEmpty()) {
        c.background = QColor(bg);
    }
    const QString fg = ini.value(QStringLiteral("colors"), QStringLiteral("foreground"));
    if (!fg.isEmpty()) {
        c.textColor = QColor(fg);
    }
    const QString dim = ini.value(QStringLiteral("colors"), QStringLiteral("dim_foreground"));
    if (!dim.isEmpty()) {
        c.dimTextColor = QColor(dim);
    }

    auto readStyle = [&ini](const QString& section, const UrgencyStyle& fallback) {
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
