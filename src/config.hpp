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
  int historyMaxEntries = 100;   // max history entries persisted to disk
  int historyRecentCount = 6;    // how many show in the compact "recent
                                 // activity" list (symm history)

  bool iconsEnabled = true; // render app/notification icon when present
  int iconSize = 40;        // px, square

  int bodyTruncateChars = 180; // body longer than this is collapsed with a
                               // "..."; click the card to expand it

  bool blurEnabled = true; // kitty-style frosted background behind cards
  int blurRadius = 24;     // gaussian blur radius in px

  // When true and running under Hyprland, use the compositor's own live
  // region blur for the overlay (layerrule blur,notifier) and keep the card
  // semi-transparent -- exactly the mechanism kitty uses. Defaults to false:
  // the app-side screenshot blur renders a guaranteed frosted backdrop on any
  // compositor without depending on Hyprland's blur being enabled.
  bool compositorBlur = false;

  bool useActiveMonitor = true; // (Hyprland only) place new notifications on
                                // the currently focused monitor at send time

  // Textures: static or animated images used in place of flat colors.
  // Animation only plays for formats Qt can decode natively (GIF/animated
  // WEBP/APNG, and AVIF if your Qt has the AVIF plugin); JXL/EXR/TIFF are
  // decoded via ffmpeg as a single static frame -- see texture.hpp.
  QString backgroundImage; // card background texture (empty = flat color)
  QString barImage;        // timer-bar fill texture (empty = flat color)
  QString iconSourceDir;   // folder to pick a random icon image from

  int maxVisible = 5; // max notifications on screen at once; 0 = unlimited.
                      // Extras queue and appear (with their timeout starting
                      // fresh) as earlier ones close.

  QString actionButtonStyle =
      QStringLiteral("grouped"); // grouped | boxed | minimal
  QString actionButtonPosition = QStringLiteral("inside"); // inside | outside
  QString barStyle = QStringLiteral("inside");             // inside | edge
  QString barPosition = QStringLiteral("below");           // above | below

  // Timer-bar fill behavior ("reverse fill").
  // barFill: false = bar drains remaining time (fills shrink with time);
  //          true = bar fills up with elapsed time instead (starts empty).
  // barMoveRight: false = fill pinned to the right edge (as it drains, the
  //               left side of the bar empties); true = fill pinned to the
  //               left edge instead.
  bool barFill = false;
  bool barMoveRight = false;

  bool backgroundImageAnchored = false; // when true, backgroundImage is
                                        // treated as one image the size of
                                        // the screen, and each card shows
                                        // the slice of it "behind" its own
                                        // position -- cards look like
                                        // windows into one shared picture
                                        // rather than each independently
                                        // stretching/cropping it.

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

  // Loads ~/.config/symm/config.conf (or $XDG_CONFIG_HOME) as the base
  // config, then layers symm.sys.ini / symm.theme.ini / symm.user.ini on top
  // (themes write symm.theme.ini). See README. Falls back to defaults.
  static Config load();
};
