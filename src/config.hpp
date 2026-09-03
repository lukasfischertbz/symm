#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

// Per-urgency accent colors.
struct UrgencyStyle {
    QColor bar;      // timer bar color
    QColor accent;   // border/accent color
};

struct Config {
    int width = 360;
    int margin = 20;
    int top = 30;
    int radius = 12;
    QString fontFamily = QStringLiteral("Cantarell");
    double fontSize = 10.5;
    int timeoutDefaultMs = 10000;  // default timeout for unclassified notifications
    int timeoutNormalMs = 5000;    // default timeout for normal notifications
    int timeoutCriticalMs = 15000; // default timeout for critical notifications
    int timerDefaultMs = 10000;    // bar drain for persistent notifications
    int historyMaxEntries = 100;   // max history entries retained
    bool persistOnMinusOne = true; // expireTimeout == -1 (e.g. `notify-send -t -1`) stays until clicked

    QColor background{0x1e, 0x1e, 0x2e, 238};
    QColor textColor{0xcd, 0xd6, 0xf4};
    QColor dimTextColor{0xa6, 0xad, 0xc8};

    UrgencyStyle low{{0x6c, 0x70, 0x86}, {0x6c, 0x70, 0x86}};
    UrgencyStyle normal{{0x89, 0xb4, 0xfa}, {0x89, 0xb4, 0xfa}};
    UrgencyStyle warning{{0xf9, 0xe2, 0xaf}, {0xf9, 0xe2, 0xaf}};
    UrgencyStyle error{{0xf3, 0x8b, 0xa8}, {0xf3, 0x8b, 0xa8}};
    UrgencyStyle critical{{0xf3, 0x8b, 0xa8}, {0xfb, 0x49, 0x34}};

    static QString urgencyColorKey(int urgency) ;

    // Loads ~/.config/symm/config.conf (or $XDG_CONFIG_HOME). Falls back to defaults.
    static Config load();
};
