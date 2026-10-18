CC ?= cc
INSTALL ?= install
.DEFAULT_GOAL := release

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-Os \
	-flto \
	-ffunction-sections \
	-fdata-sections

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

SOURCES := $(shell find src -type f -name '*.c')
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
CONFIG_STAMP := $(BUILD_DIR)/.config

.PHONY: all clean debug install quick_start release FORCE

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

quick_start: $(TARGET)
	@cat QUICK_START.md
	@printf '\nBuilding and running the example program...\n\n'
	$(TARGET) -O1 -o $(BUILD_DIR)/quick_start example/quick_start.lowc
	@$(BUILD_DIR)/quick_start
	@printf 'Done. The example ran successfully (lowC has no built-in print function yet).\n'

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

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR)
