BUILD_DIR  := build
PREFIX     ?= $(HOME)/.local
BIN        := symm

CMAKE      := cmake

.PHONY: build install uninstall clear run theme

install: build
	$(CMAKE) --install $(BUILD_DIR) --prefix $(PREFIX)
	mkdir -p $(HOME)/.config/symm

build:
	$(CMAKE) -S . -B $(BUILD_DIR) -G Ninja --preset normal
	$(CMAKE) --build $(BUILD_DIR) --preset normal

uninstall:
	rm -f $(PREFIX)/bin/$(BIN)

clear:
	-pkill -f symm || true
	-pkill -f mako || true
	-pkill -f dunst || true

run:
	setsid $(PREFIX)/bin/$(BIN) >/tmp/symm.log 2>&1 < /dev/null &

theme:
	bash themes/theme.sh

tidy:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	jq '.[].command |= gsub(" -mno-direct-extern-access"; "")' $(BUILD_DIR)/compile_commands.json > $(BUILD_DIR)/compile_commands.tidy.json
	mv $(BUILD_DIR)/compile_commands.tidy.json $(BUILD_DIR)/compile_commands.json
	run-clang-tidy -p $(BUILD_DIR) -j $(shell nproc) -header-filter=src -checks='clang-analyzer-*,bugprone-*,performance-*' $(shell find src -name '*.cpp')
