#!/usr/bin/env bash
#
# symm theme picker (`make theme`). Arrow up/down applies the highlighted
# preset immediately: its [colors] and [urgent_*] sections are merged into
# symm.user.ini in the config dir the running daemon actually reads, it pops a
# preview card in the new colors, and q quits. The daemon re-reads config on
# every notification, so no restart is needed.
# Layer order: symm.user.ini (colors via this picker) > symm.theme.ini >
# symm.sys.ini > symm.ini.
set -euo pipefail

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PREFIX=${PREFIX:-$HOME/.local}
BIN="$PREFIX/bin/symm"
LOG=/tmp/symm.log

COLORS="[colors]"
URGENT_LOW="[urgent_low]"
URGENT_NORMAL="[urgent_normal]"
URGENT_WARNING="[urgent_warning]"
URGENT_ERROR="[urgent_error]"
URGENT_CRITICAL="[urgent_critical]"

# Resolve the config dir of the running daemon from its own environment,
# falling back to the picker's env when no daemon is running.
daemon_conf_dir() {
  local pid="" xdg="" home="" env_line=""
  pid=$(pgrep -f "$BIN" | head -n1 || true)
  if [[ -n "$pid" ]] && [[ -r "/proc/$pid/environ" ]]; then
    while IFS= read -r env_line || [[ -n "$env_line" ]]; do
      case "$env_line" in
        XDG_CONFIG_HOME=*) xdg="${env_line#XDG_CONFIG_HOME=}" ;;
        HOME=*) home="${env_line#HOME=}" ;;
      esac
    done <"/proc/$pid/environ" 2>/dev/null || true
  fi
  if [[ -n "$xdg" ]]; then
    printf '%s/symm\n' "$xdg"
    return 0
  fi
  if [[ -n "$home" ]]; then
    printf '%s/.config/symm\n' "$home"
    return 0
  fi
  printf '%s\n' "${XDG_CONFIG_HOME:-$HOME/.config}/symm"
}

CONF_DIR=$(daemon_conf_dir)
USER_OUT="$CONF_DIR/symm.user.ini"
ACTIVE="$CONF_DIR/.active_theme"

shopt -s nullglob
presets=("$DIR"/*.conf)
if ((${#presets[@]} == 0)); then
  printf 'symm: no theme presets in %s\n' "$DIR" >&2
  exit 1
fi

mapfile -t THEMES < <(for p in "${presets[@]}"; do basename "$p" .conf; done | sort)
N=${#THEMES[@]}
ROWS=$((N + 1))

# Start on the currently applied theme (marker written on each apply).
sel=0
if [[ -f "$ACTIVE" ]]; then
  want=$(<"$ACTIVE")
  for i in "${!THEMES[@]}"; do
    if [[ ${THEMES[$i]} == "$want" ]]; then
      sel=$i
      break
    fi
  done
fi
applied=$sel

# Merge the preset's color sections ([colors], [urgent_*]) into symm.user.ini,
# keeping every other key already present in that file.
merge_into_user() {
  local src="$1"
  local tmp="$USER_OUT.tmp"
  : > "$tmp"
  local drop=0 line bare
  if [[ -f "$USER_OUT" ]]; then
    while IFS= read -r line || [[ -n "$line" ]]; do
      bare="${line//[$'\t ']/}"
      if [[ "$bare" == "["*"]" ]]; then
        drop=0
        case "$line" in
          *"$COLORS"*|*"$URGENT_LOW"*|*"$URGENT_NORMAL"*|*"$URGENT_WARNING"*|*"$URGENT_ERROR"*|*"$URGENT_CRITICAL"*)
            drop=1
            continue
            ;;
        esac
      fi
      if [[ $drop -eq 0 ]]; then
        printf '%s\n' "$line" >>"$tmp"
      fi
    done <"$USER_OUT"
  fi
  if [[ ! -s "$tmp" ]]; then
    printf '# symm user overrides\n' >>"$tmp"
  fi
  printf '\n' >>"$tmp"
  cat "$src" >>"$tmp"
  printf '\n' >>"$tmp"
  mv "$tmp" "$USER_OUT"
}

# Apply a preset by index, then pop a live preview card in the new colors.
apply_preset() {
  local i="$1"
  local name="${THEMES[$i]}"
  local ret=0 started=0
  CONF_DIR=$(daemon_conf_dir)
  USER_OUT="$CONF_DIR/symm.user.ini"
  ACTIVE="$CONF_DIR/.active_theme"
  mkdir -p "$CONF_DIR"
  # The daemon's Config::load() needs a base file to even start layering; if
  # it is missing (e.g. never installed) every card silently uses defaults.
  # Create an empty one so the user.ini colors are actually consulted.
  if [[ ! -e "$CONF_DIR/config.conf" && ! -e "$CONF_DIR/symm.ini" ]]; then
    : > "$CONF_DIR/config.conf"
  fi
  # Qt stores string literals as UTF-16, so decode accordingly (-e l); a
  # binary predating symm.user.ini only merits a warning, never a block.
  if command -v strings >/dev/null 2>&1 &&
     ! strings -el "$BIN" 2>/dev/null | grep -q "symm.user.ini"; then
    printf 'symm: WARNING: installed daemon looks too old for theme apply; run: make install\n' >&2
  fi
  if ! merge_into_user "$DIR/$name.conf"; then
    ret=1
  else
    printf '%s' "$name" >"$ACTIVE"
  fi
  if ! pgrep -f "$BIN" >/dev/null 2>&1; then
    setsid "$BIN" >"$LOG" 2>&1 < /dev/null &
    started=1
  fi
  if [[ $ret -eq 0 ]]; then
    if [[ $started -eq 1 ]]; then
      sleep 1
    else
      sleep 0.15
    fi
    notify-send -a symm -u normal "theme" "$name" >/dev/null 2>&1 || true
  fi
  return $ret
}

# Apply the highlighted preset in the interactive picker.
apply_current() {
  if [[ $sel == "$applied" ]] && [[ -e "$USER_OUT" ]]; then
    return 0
  fi
  if apply_preset "$sel"; then
    applied=$sel
  fi
}

render() {
  for i in "${!THEMES[@]}"; do
    if [[ $i == "$sel" ]]; then
      printf '\033[2K  > %s\033[K\n' "${THEMES[$i]}"
    else
      printf '\033[2K    %s\033[K\n' "${THEMES[$i]}"
    fi
  done
  printf '\033[2Kapplied: %s\033[K\n' "${THEMES[$applied]}"
}

redraw() {
  printf '\033[%dA' "$ROWS"
  render
}

stty_state=$(stty -g 2>/dev/null || true)
trap 'tput cnorm 2>/dev/null || true; stty sane 2>/dev/null || true; [[ -n $stty_state ]] && stty "$stty_state" 2>/dev/null || true' EXIT INT
tput civis 2>/dev/null || true
stty -icanon -echo 2>/dev/null || true

printf 'symm themes - arrow switch, q quit\n'
render
while :; do
  IFS= read -rsN1 key || break
  case "$key" in
    $'\x1b')
      IFS= read -rsN1 _ || true
      IFS= read -rsN1 code || true
      case "$code" in
        A) sel=$(((sel + N - 1) % N)); apply_current; redraw ;;
        B) sel=$(((sel + 1) % N)); apply_current; redraw ;;
      esac
      ;;
    $'\r' | $'\n' | [qQ]) printf '\nsymm: applied "%s" in %s\n' "${THEMES[$applied]}" "$CONF_DIR" >&2; exit 0 ;;
  esac
done