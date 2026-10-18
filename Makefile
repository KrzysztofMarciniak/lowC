CC ?= cc
INSTALL ?= install

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CFLAGS := \
	-std=c11 \
	-Wall \
	-Wextra \
	-Wpedantic \
	-g

CPPFLAGS := \
	-Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/lowc
EXAMPLE_DIR := $(BUILD_DIR)/example

SOURCES := $(shell find src -type f -name '*.c')
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)

.PHONY: all clean example install quick_start

all: $(TARGET)

quick_start: $(TARGET)
	@cat QUICK_START.md
	@printf '\nBuilding and running the example program...\n\n'
	$(TARGET) -O1 -o $(BUILD_DIR)/quick_start example/quick_start.lowc
	@$(BUILD_DIR)/quick_start
	@printf 'Done. The example ran successfully (lowC has no built-in print function yet).\n'

example: $(TARGET)
	@mkdir -p "$(EXAMPLE_DIR)"
	$(TARGET) --asm example/trail/main.lowc > "$(EXAMPLE_DIR)/main.s"
	$(CC) -x assembler "$(EXAMPLE_DIR)/main.s" -x c example/trail/harness.c -o "$(EXAMPLE_DIR)/trail"
	@$(EXAMPLE_DIR)/trail

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

install: $(TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 0755 "$(TARGET)" "$(DESTDIR)$(BINDIR)/lowc"

-include $(DEPFILES)

clean:
	rm -rf $(BUILD_DIR)
