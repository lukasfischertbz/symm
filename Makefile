BUILD_DIR  := build
PREFIX     ?= $(HOME)/.local
BIN        := symm

CMAKE      := cmake

.PHONY: build install uninstall clear run

install: build
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)

build:
	$(CMAKE) -S . -B $(BUILD_DIR)
	$(CMAKE) --build $(BUILD_DIR)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clear:
	-pkill -f symm || true
	-pkill -f mako || true
	-pkill -f dunst || true

run: install
	setsid $(PREFIX)/bin/$(BIN) >/tmp/symm.log 2>&1 < /dev/null &
