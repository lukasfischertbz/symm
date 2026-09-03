BUILD_DIR  := build
PREFIX     ?= $(HOME)/.local
BIN        := symm

CMAKE      := cmake

.PHONY: build install uninstall

install: build
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)

build:
	$(CMAKE) -S . -B $(BUILD_DIR)
	$(CMAKE) --build $(BUILD_DIR)

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clear:
	pkill -f symm
	pkill -f mako
	pkill -f dunst
