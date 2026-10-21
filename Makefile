CC ?= cc
INSTALL ?= install
.DEFAULT_GOAL := release

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LOWC_PUBLIC_DIR ?= $(PREFIX)/share/lowc/public
LOWC_INTERNAL_DIR ?= $(PREFIX)/share/lowc/internal
DESTDIR ?=

CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Os \
	-flto \
	-ffunction-sections \
	-fdata-sections \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables

DEBUG_CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-g

LDFLAGS ?= -Wl,--gc-sections -s

CPPFLAGS := \
	-Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/lowc
EXAMPLE_DIR := $(BUILD_DIR)/example

SOURCES := $(shell find src -type f -name '*.c')
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
CONFIG_STAMP := $(BUILD_DIR)/.config

.PHONY: all clean debug example install portable quick_start release FORCE

FORCE:

$(CONFIG_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' 'CC=$(CC)' 'CFLAGS=$(CFLAGS)' 'CPPFLAGS=$(CPPFLAGS)' 'LDFLAGS=$(LDFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || mv $@.tmp $@
	@rm -f $@.tmp

all: release

release: $(TARGET)

debug:
	$(MAKE) BUILD_DIR=build/debug CFLAGS="$(DEBUG_CFLAGS)" LDFLAGS= all

portable:
	$(MAKE) BUILD_DIR=build/portable CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS) -static" all

quick_start: $(TARGET)
	@cat QUICK_START.md
	@printf '\nBuilding and running the example program...\n\n'
	$(TARGET) -release -o $(BUILD_DIR)/quick_start example/quick_start.lowc
	@$(BUILD_DIR)/quick_start
	@printf 'Done. The example ran successfully.\n'

example: $(TARGET)
	@mkdir -p "$(EXAMPLE_DIR)"
	$(TARGET) --asm example/trail/main.lowc > "$(EXAMPLE_DIR)/main.s"
	$(CC) -x assembler "$(EXAMPLE_DIR)/main.s" -x c example/trail/harness.c -o "$(EXAMPLE_DIR)/trail"
	@$(EXAMPLE_DIR)/trail

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJECTS): $(CONFIG_STAMP)

install: release
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/lowc"
	$(INSTALL) -d "$(DESTDIR)$(LOWC_PUBLIC_DIR)"
	$(INSTALL) -m 0644 stdlib/public/args.lowc "$(DESTDIR)$(LOWC_PUBLIC_DIR)/args.lowc"
	$(INSTALL) -m 0644 stdlib/public/hashmap.lowc "$(DESTDIR)$(LOWC_PUBLIC_DIR)/hashmap.lowc"
	$(INSTALL) -m 0644 stdlib/public/memory.lowc "$(DESTDIR)$(LOWC_PUBLIC_DIR)/memory.lowc"
	$(INSTALL) -m 0644 stdlib/public/print.lowc "$(DESTDIR)$(LOWC_PUBLIC_DIR)/print.lowc"
	$(INSTALL) -m 0644 stdlib/public/string.lowc "$(DESTDIR)$(LOWC_PUBLIC_DIR)/string.lowc"
	$(INSTALL) -d "$(DESTDIR)$(LOWC_INTERNAL_DIR)/memory"
	$(INSTALL) -m 0644 stdlib/internal/memory/main.lowc "$(DESTDIR)$(LOWC_INTERNAL_DIR)/memory/main.lowc"
	$(INSTALL) -d "$(DESTDIR)$(LOWC_INTERNAL_DIR)/print"
	$(INSTALL) -m 0644 stdlib/internal/print/main.lowc "$(DESTDIR)$(LOWC_INTERNAL_DIR)/print/main.lowc"
	$(INSTALL) -d "$(DESTDIR)$(LOWC_INTERNAL_DIR)/string"
	$(INSTALL) -m 0644 stdlib/internal/string/main.lowc "$(DESTDIR)$(LOWC_INTERNAL_DIR)/string/main.lowc"

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR)
