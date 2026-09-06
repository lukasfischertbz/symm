# symm

A qt wlr-layer-shell notification daemon (DBus org.freedesktop.Notifications).

> [!NOTE]
> Made on hyprland

![](preview.avif)

![](preview.png)

```sh
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

Config is loaded from `~/.config/symm/config.conf`.
If the file is missing, defaults are used.

> [!NOTE]
> Configs are loaded for every notification

[Example](themes/minimal.conf)

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
- [x] Details
- [x] Use active monitor
- [x] Themes
- [ ] History
- [ ] Icons
- [ ] Images
