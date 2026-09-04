#pragma once

#include <QList>
#include <QPixmap>
#include <QPointer>
#include <QWidget>

#include "../config.hpp"
#include "../notification.hpp"
#include "texture.hpp"
#include "timerbarwidget.hpp"

class QLabel;
class QPushButton;
class QTimer;
class QScreen;
class QVBoxLayout;

// Single floating notification card: an independent layer-shell surface so the
// compositor can blur it as one rectangle (kitty-style frost). Each card is its
// own window; the manager stacks them vertically via margins. Timed
// notifications show a draining bar over their timeout and auto-dismiss when
// it empties; persistent ones (the persistence hint, or expire 0) show no bar
// and stay until clicked. Hovering a truncated body previews the full text.
// Notifications carrying action keys get a row of buttons that emit
// actionInvoked(id, key) when clicked.
class NotificationWindow : public QWidget {
  Q_OBJECT
public:
  explicit NotificationWindow(const Notification &n, const Config &cfg,
                              QScreen *targetScreen = nullptr,
                              QWidget *parent = nullptr);

  // Top offset (px) from the screen top; used to stack multiple notifications.
  void setTopOffset(int topMargin);

  uint id() const { return m_id; }
  // The output this card was pinned to at send time (see
  // NotificationManager::show / "use active monitor"). Null means "whatever
  // the default/primary screen is" -- reflow() groups by this so two
  // monitors each get their own independent stack instead of sharing one
  // running height counter.
  QScreen *targetScreen() const { return m_targetScreen; }

signals:
  void dismissed(uint id);
  void actionInvoked(uint id, const QString &key);
  void resized();

protected:
  void paintEvent(QPaintEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private slots:
  void onTimeoutFinished();
  void onActionClicked(const QString &key);
  void onBgFrameTick();

private:
  void layoutContents(const Notification &n);
  void layoutActions(const QStringList &actions);
  void setupLayerShell();
  void updateBlurPanel();
  // Shared tail of hover-expand: re-measures the (now taller-or-shorter)
  // content, resizes the window, and tells the manager to reflow so cards
  // below shift accordingly.
  void relayoutForBodyChange();

  uint m_id;
  Config m_cfg;
  QScreen *m_targetScreen = nullptr;
  UrgencyStyle m_style;
  QPointer<QTimer> m_lifeTimer;
  TimerBarWidget *m_timerBar = nullptr;
  QLabel *m_iconLabel = nullptr;
  QLabel *m_summaryLabel = nullptr;
  QLabel *m_bodyLabel = nullptr;
  QList<QPushButton *> m_actionButtons;
  // The padded inner column (icon/summary/body/inside-actions/inside-bar).
  // A separate 0-margin outer layout wraps this plus anything configured to
  // sit at the literal card edge (bar_style=edge) or outside the card panel
  // entirely (action_button_position=outside).
  QVBoxLayout *m_contentLayout = nullptr;
  QWidget *m_actionsRowWidget = nullptr;
  bool m_actionsOutside = false;

  // "Details" (hover to preview truncated body).
  QString m_fullBody;
  QString m_truncatedBody;
  bool m_truncated = false;
  bool m_hoverTemporaryExpand = false; // transient, collapses again on leave

  // True while relayoutForBodyChange() is running. On Wayland a window resize
  // can synthesize enter/leave events, which would collapse a hover-expanded
  // body immediately (relayout -> leave -> shrink -> enter -> expand...). The
  // event handlers ignore enter/leave while this is set.
  bool m_inRelayout = false;

  // Hover pauses the auto-dismiss countdown; this is the remaining time
  // (ms) captured at the moment the pointer entered, restored on leave.
  int m_pausedRemainingMs = 0;

  // Cached frosted-glass backdrop; regenerated on show/resize, not every
  // paint (a real screen grab + gaussian blur is too slow to do per-frame).
  QPixmap m_blurPanel;

  // Background texture (config.backgroundImage). Animates via m_bgAnimTimer
  // when the source has more than one frame (GIF/animated WEBP/APNG, or
  // AVIF with plugin support -- see texture.hpp for what animates vs. not).
  QList<TextureFrame> m_bgFrames;
  int m_bgFrameIndex = 0;
  QPointer<QTimer> m_bgAnimTimer;
};
