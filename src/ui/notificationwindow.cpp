#include "notificationwindow.hpp"

#include <LayerShellQt/Window>

#include <QApplication>
#include <QBoxLayout>
#include <QEnterEvent>
#include <QFlags>
#include <QLabel>
#include <QMargins>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QStringList>
#include <QTimer>

#include "blur.hpp"
#include "texture.hpp"

namespace {
// Break a single run of text into lines that fit within maxWidth px. Wraps at
// word boundaries like QLabel's word-wrap (which it mirrors), but splits long
// space-less runs (URLs, hashes) mid-word so they never overflow. Explicit
// newlines in the source are preserved.
QString wrapPlainText(const QFont &f, const QString &text, int maxWidth) {
  if (text.isEmpty() || maxWidth <= 0) {
    return text;
  }
  const QFontMetrics fm(f);
  QString result;
  QString line;
  QString word;
  bool pendingSpace = false;

  auto flushWord = [&]() {
    if (word.isEmpty()) {
      return;
    }
    const bool fitsNext =
        line.isEmpty()
            ? fm.horizontalAdvance(word) <= maxWidth
            : fm.horizontalAdvance(line + QLatin1Char(' ') + word) <= maxWidth;
    if (fitsNext) {
      if (pendingSpace && !line.isEmpty()) {
        line += QLatin1Char(' ');
      }
      line += word;
    } else {
      if (!line.isEmpty()) {
        result += line + QLatin1Char('\n');
        line.clear();
      }
      // The word alone overflows the width (URLs, hashes): hard-break it
      // character by character so it still fits.
      if (fm.horizontalAdvance(word) > maxWidth) {
        QString piece;
        for (const QChar &ch : word) {
          if (fm.horizontalAdvance(piece + ch) > maxWidth && !piece.isEmpty()) {
            result += piece + QLatin1Char('\n');
            piece = ch;
          } else {
            piece += ch;
          }
        }
        line = piece;
      } else {
        line = word;
      }
    }
    word.clear();
    pendingSpace = true;
  };

  for (const QChar &ch : text) {
    if (ch == QLatin1Char('\n')) {
      flushWord();
      result += line + QLatin1Char('\n');
      line.clear();
      pendingSpace = false;
      continue;
    }
    if (ch.isSpace()) {
      flushWord();
      continue;
    }
    word += ch;
  }
  flushWord();
  result += line;
  return result;
}
} // namespace

NotificationWindow::NotificationWindow(const Notification &n, const Config &cfg,
                                       QScreen *targetScreen, QWidget *parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint),
      m_id(n.id), m_appName(n.appName), m_summary(n.summary), m_cfg(cfg),
      m_targetScreen(targetScreen) {
  // Pick the accent style for this notification's urgency.
  const QString key = m_cfg.urgencyColorKey(n.urgency);
  if (key == QStringLiteral("low")) {
    m_style = m_cfg.low;
  } else if (key == QStringLiteral("critical")) {
    m_style = m_cfg.critical;
  } else {
    m_style = m_cfg.normal;
  }

  setAttribute(Qt::WA_TranslucentBackground);
  setWindowTitle(QStringLiteral("notifier"));
  setAttribute(Qt::WA_Hover);
  setMouseTracking(true);

  setFixedWidth(m_cfg.width);

  if (!m_cfg.backgroundImage.isEmpty()) {
    m_bgFrames = loadTextureFrames(m_cfg.backgroundImage);
    if (m_bgFrames.size() > 1) {
      m_bgAnimTimer = new QTimer(this);
      m_bgAnimTimer->setSingleShot(true);
      connect(m_bgAnimTimer, &QTimer::timeout, this,
              &NotificationWindow::onBgFrameTick);
      m_bgAnimTimer->start(qMax(20, m_bgFrames.first().delayMs));
    }
  }

  buildContent(n);

  if (targetScreen != nullptr) {
    // Force native window creation now so we can pin it to a specific
    // output before the layer-shell surface is bound in showEvent().
    winId();
    if (QWindow *handle = windowHandle()) {
      handle->setScreen(targetScreen);
    }
  }
  show();
}

void NotificationWindow::setupLayerShell() {
  QWindow *handle = windowHandle();
  if (!handle) {
    return;
  }
  LayerShellQt::Window *shell = LayerShellQt::Window::get(handle);
  if (!shell) {
    return;
  }
  shell->setLayer(LayerShellQt::Window::LayerOverlay);
  shell->setScope(QStringLiteral("notifier"));
  using Anchor = LayerShellQt::Window::Anchor;
  shell->setAnchors(QFlags<Anchor>(Anchor::AnchorTop) |
                    QFlags<Anchor>(Anchor::AnchorRight));
  // Margins are owned exclusively by setTopOffset() (the manager's reflow)
  // -- never set them here, or the first show clobbers the stacked position.
  shell->setExclusiveZone(-1); // no reserved space, floats over content
  shell->setKeyboardInteractivity(
      LayerShellQt::Window::KeyboardInteractivityNone);
}

void NotificationWindow::showEvent(QShowEvent *event) {
  setupLayerShell();
  updateBlurPanel();
  QWidget::showEvent(event);
}

void NotificationWindow::updateBlurPanel() {
  // Compositor-side (Hyprland layerrule) blur paints no backdrop pixmap: the
  // card just stays translucent and the compositor blurs the live desktop
  // behind it each frame. Skip the (expensive) screenshot pipeline entirely.
  const bool compositorBlur = m_cfg.compositorBlur && runningOnHyprland();
  if (!m_cfg.blurEnabled || compositorBlur || size().isEmpty() ||
      !m_bgFrames.isEmpty()) {
    // A texture background (if set) always wins over blur -- see
    // paintEvent -- so skip the (relatively expensive) capture entirely.
    m_blurPanel = QPixmap();
    return;
  }
  // A real screen grab + gaussian blur is too slow to redo every paint, so
  // it's cached here and only regenerated on show/resize/expand.
  const QRect globalRect(mapToGlobal(QPoint(0, 0)), size());
  m_blurPanel =
      makeFrostedPanel(globalRect, m_cfg.blurRadius, m_cfg.background);
  update();
}

void NotificationWindow::setTopOffset(int topMargin) {
  const int m = m_cfg.margin;
  if (QWindow *handle = windowHandle()) {
    if (LayerShellQt::Window *shell = LayerShellQt::Window::get(handle)) {
      shell->setMargins(QMargins(0, topMargin, m, 0));
      // LayerShellQt only re-commits the surface when a redraw is scheduled;
      // a pure margin change (stack shift, e.g. the card above closing) would
      // otherwise never reach the compositor and the card wouldn't move up.
      handle->requestUpdate();
    }
  }
}

void NotificationWindow::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateBlurPanel();
  emit resized();
}

void NotificationWindow::moveEvent(QMoveEvent *event) {
  QWidget::moveEvent(event);
  // The compositor assigns the final position only after the layer-shell
  // surface is configured, so the first showEvent capture may have used
  // stale coordinates (see blur.hpp). Re-crop the backdrop now that the card
  // actually sits where it will be drawn.
  updateBlurPanel();
  update();
}

void NotificationWindow::mousePressEvent(QMouseEvent *event) {
  // One click always dismisses; the full body is previewed on hover instead
  // of expanding on click (so a click never needs a second one to go away).
  emit dismissed(m_id);
  close();
  QWidget::mousePressEvent(event);
}

void NotificationWindow::enterEvent(QEnterEvent *event) {
  // Resize-triggered enter/leave bursts (Wayland) must not be treated as the
  // pointer actually arriving; see m_inRelayout.
  if (m_inRelayout) {
    QWidget::enterEvent(event);
    return;
  }

  // Hovering halts the auto-dismiss countdown -- both the QTimer driving it
  // and the bar animating it -- and resumes from exactly where it left off
  // when the pointer leaves (see leaveEvent), rather than losing the
  // notification mid-read.
  if (m_lifeTimer && m_lifeTimer->isActive()) {
    m_pausedRemainingMs = m_lifeTimer->remainingTime();
    m_lifeTimer->stop();
    if (m_timerBar) {
      m_timerBar->pause();
    }
  }

  // Hovering a truncated body also previews the full text, same as a click
  // but temporary: it collapses back on leaveEvent (see
  // m_hoverTemporaryExpand).
  if (m_truncated && !m_hoverTemporaryExpand && m_bodyLabel != nullptr) {
    m_hoverTemporaryExpand = true;
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    const int textMaxW =
        m_cfg.width - 2 * m_cfg.paddingH -
        (m_iconLabel != nullptr ? m_cfg.iconSize + m_cfg.gap + 4 : 0);
    m_bodyLabel->setText(wrapPlainText(f, m_fullBody, textMaxW));
    relayoutForBodyChange();
  }

  QWidget::enterEvent(event);
}

void NotificationWindow::leaveEvent(QEvent *event) {
  if (m_inRelayout) {
    QWidget::leaveEvent(event);
    return;
  }

  if (m_lifeTimer != nullptr && !m_lifeTimer->isActive()) {
    // Resume even when 0 was captured: a hover landing in the final instant
    // (remainingTime() == 0) must still re-arm the dismiss timer, otherwise
    // the notification stays frozen forever (QTimer::start(0) fires on the
    // next event loop turn, which is exactly when it should have gone).
    m_lifeTimer->start(m_pausedRemainingMs);
    if (m_timerBar != nullptr) {
      m_timerBar->resume();
    }
    m_pausedRemainingMs = 0;
  }

  if (m_hoverTemporaryExpand && m_bodyLabel != nullptr) {
    m_hoverTemporaryExpand = false;
    // Only the temporary hover preview shrinks back down on leave.
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    const int textMaxW =
        m_cfg.width - 2 * m_cfg.paddingH -
        (m_iconLabel != nullptr ? m_cfg.iconSize + m_cfg.gap + 4 : 0);
    m_bodyLabel->setText(wrapPlainText(f, m_truncatedBody, textMaxW));
    relayoutForBodyChange();
  }

  QWidget::leaveEvent(event);
}

void NotificationWindow::relayoutForBodyChange() {
  m_inRelayout = true;
  auto *outer = static_cast<QVBoxLayout *>(this->layout());
  if (outer == nullptr) {
    m_inRelayout = false;
    return;
  }
  outer->activate();
  const int contentH = outer->sizeHint().height();
  // Reuse the same per-type minimum as the constructor (timed cards keep the
  // 70px floor; bar-free cards hug their content).
  const bool timed = m_lifeTimer != nullptr;
  const int minH = timed ? 70 : m_cfg.paddingV * 2 + m_cfg.gap;
  setFixedSize(m_cfg.width, qMax(contentH, minH));

  updateBlurPanel();
  emit resized();
  m_inRelayout = false;
}

void NotificationWindow::layoutContents(const Notification &n) {
  // Outer, 0-margin layout: holds the padded content column plus anything
  // configured to sit flush at the literal card edge (bar_style=edge) or
  // detached below the card panel (action_button_position=outside).
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  m_contentLayout = new QVBoxLayout;
  m_contentLayout->setContentsMargins(m_cfg.paddingH, m_cfg.paddingV,
                                      m_cfg.paddingH, m_cfg.paddingV);
  m_contentLayout->setSpacing(m_cfg.gap);

  const QString dim = m_cfg.dimTextColor.name();
  const QString fg = m_cfg.textColor.name();

  // Icon (left) + summary/body (right), side by side.
  auto *row = new QHBoxLayout;
  row->setSpacing(m_cfg.gap + 4);

  if (m_cfg.iconsEnabled && !n.icon.isNull()) {
    m_iconLabel = new QLabel(this);
    const int sz = m_cfg.iconSize;
    m_iconLabel->setFixedSize(sz, sz);
    m_iconLabel->setPixmap(n.icon.pixmap(sz, sz));
    m_iconLabel->setScaledContents(true);
    m_iconLabel->setStyleSheet(QStringLiteral("background: transparent;"));
    row->addWidget(m_iconLabel, 0, Qt::AlignTop);
  }

  auto *textCol = new QVBoxLayout;
  textCol->setSpacing(m_cfg.gap);

  // Cap the text column width so that word wrap triggers even on long words
  // without spaces. Without this Qt measures the label at its full intrinsic
  // width before the outer widget's setFixedSize constraint is applied.
  const int textMaxW =
      m_cfg.width - 2 * m_cfg.paddingH -
      (m_cfg.iconsEnabled && !n.icon.isNull() ? m_cfg.iconSize + m_cfg.gap + 4
                                              : 0);

  if (!n.summary.isEmpty()) {
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setText(n.summary);
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    f.setBold(true);
    m_summaryLabel->setFont(f);
    m_summaryLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(fg));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setFixedWidth(textMaxW);
    textCol->addWidget(m_summaryLabel);
  }

  if (!n.body.isEmpty()) {
    // Normalize CRLF / lone CR to plain LF. Bodies arriving over D-Bus from
    // Windows-origin apps carry \r\n, which QLabel's PlainText renderer does
    // not collapse into line breaks -- it shows stray glyphs instead.
    QString body = n.body;
    body.remove(QLatin1Char('\r'));
    m_fullBody = body;
    m_truncated = body.size() > qMax(0, m_cfg.bodyTruncateChars);
    m_truncatedBody =
        m_truncated
            ? body.left(m_cfg.bodyTruncateChars)
                      .remove(QRegularExpression(QStringLiteral("\\s+$"))) +
                  QStringLiteral("…")
            : body;

    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setTextFormat(Qt::PlainText);
    QFont f(m_cfg.fontFamily, static_cast<int>(m_cfg.fontSize));
    m_bodyLabel->setFont(f);
    m_bodyLabel->setText(wrapPlainText(f, m_truncatedBody, textMaxW));
    m_bodyLabel->setStyleSheet(
        QStringLiteral("color: %1; background: transparent;").arg(dim));
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setFixedWidth(textMaxW);
    textCol->addWidget(m_bodyLabel);
  }

  row->addLayout(textCol, 1);
  m_contentLayout->addLayout(row);

  outer->addLayout(m_contentLayout);

  layoutActions(n.actions);
}

void NotificationWindow::layoutActions(const QStringList &actions) {
  if (actions.size() < 2) {
    return;
  }
  auto *outer = static_cast<QVBoxLayout *>(this->layout());
  if (outer == nullptr || m_contentLayout == nullptr) {
    return;
  }

  m_actionsOutside = m_cfg.actionButtonPosition == QStringLiteral("outside");
  // "outside" buttons have no shared card panel behind them (paintEvent
  // shrinks the card path to exclude this row -- see there), so they always
  // render as individual pills regardless of action_button_style; grouped/
  // minimal only apply to buttons drawn inside the card.
  const QString style =
      m_actionsOutside ? QStringLiteral("boxed") : m_cfg.actionButtonStyle;
  const QString accent = m_style.accent.name();

  auto *rowWidget = new QWidget(this);
  rowWidget->setAttribute(Qt::WA_TranslucentBackground);
  auto *row = new QHBoxLayout(rowWidget);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(style == QStringLiteral("grouped") ? 0 : 6);

  QStringList keys;
  QStringList labels;
  for (int i = 0; i + 1 < actions.size(); i += 2) {
    keys.append(actions.at(i));
    labels.append(actions.at(i + 1));
  }
  const int count = static_cast<int>(keys.size());

  for (int i = 0; i < count; ++i) {
    const QString &key = keys.at(i);
    const QString label = labels.at(i).isEmpty() ? key : labels.at(i);
    auto *btn = new QPushButton(label, rowWidget);
    btn->setCursor(Qt::PointingHandCursor);

    QString css;
    if (style == QStringLiteral("minimal")) {
      css =
          QStringLiteral("QPushButton { color: %1; background: transparent; "
                         "border: none; padding: 4px 10px; font-weight: 600; }"
                         "QPushButton:hover { color: white; }"
                         "QPushButton:pressed { color: %1; }")
              .arg(accent);
    } else if (style == QStringLiteral("grouped")) {
      QString corners;
      if (count == 1) {
        corners = QStringLiteral("border-radius: 6px;");
      } else if (i == 0) {
        corners = QStringLiteral("border-top-left-radius: 6px; "
                                 "border-bottom-left-radius: 6px; "
                                 "border-top-right-radius: 0; "
                                 "border-bottom-right-radius: 0;");
      } else if (i == count - 1) {
        corners = QStringLiteral("border-top-right-radius: 6px; "
                                 "border-bottom-right-radius: 6px; "
                                 "border-top-left-radius: 0; "
                                 "border-bottom-left-radius: 0;");
      } else {
        corners = QStringLiteral("border-radius: 0;");
      }
      const QString leftBorder =
          i == 0 ? QStringLiteral("border-left: 1px solid %1;").arg(accent)
                 : QStringLiteral("border-left: none;");
      css = QStringLiteral(
                "QPushButton { color: %1; background: "
                "rgba(255,255,255,18); border: 1px solid %1; %2 %3 "
                "padding: 4px 12px; }"
                "QPushButton:hover { background: rgba(255,255,255,38); }"
                "QPushButton:pressed { background: rgba(255,255,255,55); }")
                .arg(accent, leftBorder, corners);
    } else {
      css = QStringLiteral(
                "QPushButton { color: %1; background: transparent; "
                "border: 1px solid %1; border-radius: 4px; padding: 3px 8px; }"
                "QPushButton:hover { background: rgba(255,255,255,20); }"
                "QPushButton:pressed { background: rgba(255,255,255,38); }")
                .arg(accent);
    }
    btn->setStyleSheet(css);
    connect(btn, &QPushButton::clicked, this,
            [this, key] { onActionClicked(key); });
    row->addWidget(btn);
    m_actionButtons.append(btn);
  }

  m_actionsRowWidget = rowWidget;
  if (m_actionsOutside) {
    outer->addSpacing(6);
    outer->addWidget(rowWidget, 0, Qt::AlignHCenter);
  } else {
    m_contentLayout->addWidget(rowWidget);
  }
}

void NotificationWindow::buildContent(const Notification &n) {
  // Teardown of any previous content. Everything here is either null on the
  // first call (fresh card) or owned by this widget, so this doubles as the
  // update-in-place path for replaced notifications.
  if (m_lifeTimer != nullptr) {
    m_lifeTimer->stop();
    m_lifeTimer->deleteLater();
    m_lifeTimer = nullptr;
  }
  if (m_timerBar != nullptr) {
    delete m_timerBar;
    m_timerBar = nullptr;
  }
  delete m_iconLabel;
  m_iconLabel = nullptr;
  delete m_summaryLabel;
  m_summaryLabel = nullptr;
  delete m_bodyLabel;
  m_bodyLabel = nullptr;
  delete m_actionsRowWidget;
  m_actionsRowWidget = nullptr;
  m_actionButtons.clear();
  m_fullBody.clear();
  m_truncatedBody.clear();
  m_truncated = false;
  m_hoverTemporaryExpand = false;
  m_pausedRemainingMs = 0;

  // Delete the old layout (labels/buttons/widgets were deleted above; the
  // layout only holds the items/sub-layouts, which are owned by it).
  delete layout();
  layoutContents(n);

  auto *outer = static_cast<QVBoxLayout *>(this->layout());

  // The bar tracks the remaining time on notifications that auto-dismiss:
  // timed ones drain over their exact timeout. Persistent ones (persistence
  // hint / expire 0) stay until clicked; they get no bar at all -- the bar
  // widget is only mounted into the layout when the notification is timed,
  // so it can neither render nor reserve dead space.
  const bool timed = !n.persist && n.timeoutMs > 0;

  m_timerBar = new TimerBarWidget(this);
  m_timerBar->setBarColor(m_style.bar);
  if (!m_cfg.barImage.isEmpty()) {
    m_timerBar->setBarImage(m_cfg.barImage);
  }
  const bool edgeBar = m_cfg.barStyle == QStringLiteral("edge");
  const bool barAbove = m_cfg.barPosition == QStringLiteral("above");
  m_timerBar->setEdgeStyle(edgeBar);
  m_timerBar->setFillUp(m_cfg.barFill);
  m_timerBar->setMoveRight(m_cfg.barMoveRight);
  m_timerBar->setVisible(timed);

  if (timed) {
    if (edgeBar) {
      // Flush with the card's literal border, no padding -- goes in the
      // 0-margin outer layout, not the padded content column.
      if (barAbove) {
        outer->insertWidget(0, m_timerBar);
      } else {
        outer->addWidget(m_timerBar);
      }
    } else {
      // "inside" (default): padded like everything else, positioned above or
      // below the message text within the content column.
      if (barAbove) {
        m_contentLayout->insertWidget(0, m_timerBar);
      } else {
        m_contentLayout->addWidget(m_timerBar);
      }
    }
  }

  outer->activate();
  int contentH = outer->sizeHint().height();
  // Minimum height: timed cards keep a comfortable floor; bar-free cards
  // size to their content so they don't inherit bar dead space.
  const int minH = timed ? 70 : m_cfg.paddingV * 2 + m_cfg.gap;
  setFixedSize(m_cfg.width, qMax(contentH, minH));

  if (timed) {
    m_lifeTimer = new QTimer(this);
    m_lifeTimer->setInterval(n.timeoutMs);
    connect(m_lifeTimer, &QTimer::timeout, this,
            &NotificationWindow::onTimeoutFinished);
    m_lifeTimer->start();
    m_timerBar->start(n.timeoutMs);
  }
  // Persistent notifications get no life timer and no bar: they stay on
  // screen until the user clicks them (or an action / CloseNotification is
  // invoked). Hover-pause is a no-op for them because m_lifeTimer is null.
}

void NotificationWindow::updateFrom(const Notification &n) {
  m_appName = n.appName;
  m_summary = n.summary;

  // An update can change urgency, so re-pick the accent style.
  const QString key = m_cfg.urgencyColorKey(n.urgency);
  if (key == QStringLiteral("low")) {
    m_style = m_cfg.low;
  } else if (key == QStringLiteral("critical")) {
    m_style = m_cfg.critical;
  } else {
    m_style = m_cfg.normal;
  }

  // Keep the existing icon if the update doesn't ship one (common with audio
  // progress updates) -- otherwise the card would visibly drop its icon and
  // jump on every refresh.
  Notification effective = n;
  if (effective.icon.isNull() && m_iconLabel != nullptr && m_cfg.iconsEnabled) {
    effective.icon = QIcon(m_iconLabel->pixmap());
  }

  buildContent(effective);
  updateBlurPanel();
  emit resized(); // the manager reflows whatever sits below this card
}

void NotificationWindow::onActionClicked(const QString &key) {
  emit actionInvoked(m_id, key);
}

void NotificationWindow::onBgFrameTick() {
  if (m_bgFrames.isEmpty()) {
    return;
  }
  m_bgFrameIndex = (m_bgFrameIndex + 1) % static_cast<int>(m_bgFrames.size());
  update();
  if (m_bgAnimTimer) {
    m_bgAnimTimer->start(qMax(20, m_bgFrames[m_bgFrameIndex].delayMs));
  }
}

void NotificationWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // When action buttons render "outside", they sit below the card panel
  // proper -- shrink the painted panel to exclude that strip instead of
  // covering it.
  QRect cardRect = rect();
  if (m_actionsOutside && m_actionsRowWidget != nullptr &&
      m_actionsRowWidget->height() > 0) {
    const int excl = m_actionsRowWidget->height() + 6;
    cardRect.setHeight(qMax(20, height() - excl));
  }

  QPainterPath path;
  path.addRoundedRect(cardRect, m_cfg.radius, m_cfg.radius);

  if (!m_bgFrames.isEmpty()) {
    const QImage &frame = m_bgFrames[m_bgFrameIndex].image;
    QPixmap toDraw;
    QRect src;

    if (m_cfg.backgroundImageAnchored) {
      QScreen *scr = m_targetScreen != nullptr ? m_targetScreen : screen();
      const QSize screenSize =
          scr != nullptr ? scr->geometry().size() : QSize(1920, 1080);
      const QPoint screenOrigin =
          scr != nullptr ? scr->geometry().topLeft() : QPoint(0, 0);
      toDraw = QPixmap::fromImage(frame).scaled(
          screenSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      const QPoint winPos = mapToGlobal(QPoint(0, 0)) - screenOrigin;
      src = QRect(winPos, cardRect.size()).intersected(toDraw.rect());
      if (src.isEmpty()) {
        src = QRect(0, 0, qMin(toDraw.width(), cardRect.width()),
                    qMin(toDraw.height(), cardRect.height()));
      }
    } else {
      // Crop-to-fill (like CSS background-size: cover), independent per card.
      toDraw = QPixmap::fromImage(frame).scaled(cardRect.size(),
                                                Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
      src = QRect((toDraw.width() - cardRect.width()) / 2,
                  (toDraw.height() - cardRect.height()) / 2, cardRect.width(),
                  cardRect.height());
    }

    p.setClipPath(path);
    p.drawPixmap(cardRect, toDraw, src);
    p.setClipping(false);

    QColor tint = m_cfg.background;
    tint.setAlphaF(
        static_cast<float>(tint.alphaF() * m_cfg.backgroundOpacity * 0.5));
    p.fillPath(path, tint);
  } else if (m_cfg.blurEnabled && runningOnHyprland() && m_cfg.compositorBlur) {
    // Hyprland compositor blur (the kitty mechanism): Hyprland blurs the live
    // desktop behind the whole layer surface; the card only lays a translucent
    // tint on top so the frosted content shows through. TEXT IS UNTOUCHED --
    // the child widgets draw as solid opaque pixels on the surface and are
    // never part of the blur.
    p.setClipPath(path);
    QColor tint = m_cfg.background;
    const float base = static_cast<float>(tint.alphaF());
    const float a = base * static_cast<float>(m_cfg.backgroundOpacity) * 0.40f;
    tint.setAlphaF(qBound(0.0f, a, 1.0f));
    p.fillPath(path, tint);
    p.setClipping(false);
  } else if (m_cfg.blurEnabled && !m_blurPanel.isNull()) {
    p.setClipPath(path);
    p.drawPixmap(0, 0, m_blurPanel);
    p.setClipping(false);

    // Kitty-style: the blurred desktop shows through almost unobscured. Only a
    // faint darkening is applied so text stays legible -- never an opaque
    // color wash (that is what made it read as a flat colored panel instead of
    // real blurred content behind the card).
    QColor tint = m_cfg.background;
    const float base = static_cast<float>(tint.alphaF());
    const float a = base * static_cast<float>(m_cfg.backgroundOpacity) * 0.20f;
    tint.setAlphaF(qBound(0.0f, a, 1.0f));
    p.fillPath(path, tint);
  } else {
    QColor fill = m_cfg.background;
    fill.setAlphaF(static_cast<float>(fill.alphaF() * m_cfg.backgroundOpacity));
    p.fillPath(path, fill);
  }

  QColor border = m_style.accent;
  border.setAlpha(150);
  p.setPen(QPen(border, 1));
  p.drawPath(path);
}

void NotificationWindow::onTimeoutFinished() {
  emit dismissed(m_id);
  close();
}
