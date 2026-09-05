# symm

A qt wlr-layer-shell notification daemon (DBus org.freedesktop.Notifications).

> [!NOTE]
> Made on hyprland

![](preview.avif)

![](preview.png)

```sh
# cd /tmp
git clone https://github.com/lukasfischertbz/symm.git
cd symm
make install clear run
```

Test

```sh
notify-send -u low "low" "Message"
notify-send -u normal "normal" "Message"
notify-send -u critical "critical" "Message"

notify-send "" "1\n2"
notify-send -h string:persistence:true "Click me"
```

---

## Build

Requires Qt6 (Core Gui Widgets DBus) and Layer Shell Qt.

```sh
make build
```

---

## Install

```sh
make
```

## Uninstall

```sh
make uninstall
```

---

## Run

```sh
symm & disown
```

Starts the DBus notification server and displays floating notifications.

---

## Config

Config is loaded from `~/.config/symm/config.conf` (respects `$XDG_CONFIG_HOME`).
If the file is missing, defaults are used.

> [!NOTE]
> Configs are loaded for every notification

[Example](config.conf)

```ini
[general]
width = 360
margin = 20
radius = 12
font_family = Cantarell
font_size = 10.5
timeout_low = 10000
timeout_normal = 5000
timeout_critical = 15000
# Notifications sent with -h string:persistence:true (or notify-send -t 0)
# never auto-dismiss -- they stay until clicked.

[colors]
background = #1e1e2e
foreground = #cdd6f4
dim_foreground = #a6adc8

[urgent_low]
bar = #6c7086
accent = #6c7086

[urgent_normal]
# ...
```

### Themes

Test presets

```sh
make theme
```

Hirarchy is

`symm.user.ini` > `symm.theme.ini` > `symm.sys.ini` > `symm.ini`

---

## Features

- [x] Timeout visualizer
- [x] Actions
- [x] Transparency
- [x] Details (click a truncated notification to expand it)
- [x] Use active monitor (Hyprland; on send only)
- [x] Themes
- [ ] History
- [ ] Icons
- [ ] App-side blur (kitty-style frosted background, any compositor)
- [ ] Images
