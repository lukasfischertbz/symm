#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

// Per-urgency accent colors.
struct UrgencyStyle {
  QColor bar;    // timer bar color
  QColor accent; // border/accent color
};

struct Config {
  int width = 360;
  int margin = 20;
  int top = 30;
  int radius = 12;
  int paddingH = 16;    // horizontal padding inside a card
  int paddingV = 8;     // vertical padding inside a card
  int gap = 6;          // vertical spacing between elements inside a card
  int cardSpacing = 18; // vertical gap between stacked cards
  QString fontFamily = QStringLiteral("Cantarell");
  double fontSize = 10.5;
  int timeoutDefaultMs =
      10000;                  // default timeout for unclassified notifications
  int timeoutNormalMs = 5000; // default timeout for normal notifications
  int timeoutCriticalMs = 15000; // default timeout for critical notifications
  int timerDefaultMs = 10000;    // bar drain for persistent notifications
  int historyMaxEntries = 100;   // max history entries retained
  bool persistOnMinusOne = true; // expireTimeout == -1 (e.g. `notify-send -t
                                 // -1`) stays until clicked

  bool iconsEnabled = true; // render app/notification icon when present
  int iconSize = 40;        // px, square

  int bodyTruncateChars = 180; // body longer than this is collapsed with a
                               // "..."; click the card to expand it

  bool blurEnabled = true; // kitty-style frosted background behind cards
  int blurRadius = 24;     // gaussian blur radius in px

  bool useActiveMonitor = true; // (Hyprland only) place new notifications on
                                // the currently focused monitor at send time
  bool barMoveRight = false;    // grow the bar toward the right edge
  bool barReverse = false;      // reverse the fill direction over time
  bool barFill = true;          // start full and drain toward zero

  QColor background{0x1e, 0x1e, 0x2e, 238};
  double backgroundOpacity = 1.0; // kitty-style: 1.0 opaque, 0.0 transparent
  QColor textColor{0xcd, 0xd6, 0xf4};
  QColor dimTextColor{0xa6, 0xad, 0xc8};

  UrgencyStyle low{{0x6c, 0x70, 0x86}, {0x6c, 0x70, 0x86}};
  UrgencyStyle normal{{0x89, 0xb4, 0xfa}, {0x89, 0xb4, 0xfa}};
  UrgencyStyle warning{{0xf9, 0xe2, 0xaf}, {0xf9, 0xe2, 0xaf}};
  UrgencyStyle error{{0xf3, 0x8b, 0xa8}, {0xf3, 0x8b, 0xa8}};
  UrgencyStyle critical{{0xf3, 0x8b, 0xa8}, {0xfb, 0x49, 0x34}};

  static QString urgencyColorKey(int urgency);

  // Loads ~/.config/symm/config.conf (or $XDG_CONFIG_HOME). Falls back to
  // defaults.
  static Config load();
};
