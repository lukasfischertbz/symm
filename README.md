# symm

A wlr-layer-shell notification daemon (DBus org.freedesktop.Notifications).

## Build

Requires Qt6 (Core Gui Widgets DBus) and Layer Shell Qt.

```sh
cmake -S . -B build
cmake --build build
```

## Install

```sh
cmake --install build --prefix ~/.local   # ~/.local/bin/symm
```

## Run

```sh
~/.local/bin/symm &
notify-send -t 8000 test hi
```

Starts the DBus notification server and displays floating notifications.

## Config

Config is loaded from `~/.config/symm/config.conf` (respects `$XDG_CONFIG_HOME`).
If the file is missing, defaults are used.

INI-style (`[section]` headers, `key = value`, `#`/`;` comments):

```ini
[general]
width = 360
margin = 20
radius = 12
font_family = Cantarell
font_size = 10.5
timeout_default = 10000
timeout_normal = 5000
timeout_critical = 15000
timer_default = 10000

[colors]
background = #1e1e2e
foreground = #cdd6f4
dim_foreground = #a6adc8

[urgent_low]
bar = #6c7086
accent = #6c7086

[urgent_normal]
bar = #89b4fa
accent = #89b4fa

[urgent_warning]
bar = #f9e2af
accent = #f9e2af

[urgent_error]
bar = #f38ba8
accent = #f38ba8

[urgent_critical]
bar = #f38ba8
accent = #fb4934
```

Section `urgent_error` applies to unclassified notifications; `urgent_normal`
to normal; `urgent_critical` to critical. Colors accept any QColor string.
