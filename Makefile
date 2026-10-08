# NanoRDS: one GNU Makefile for Linux and Windows (MSYS2 MinGW/UCRT64).
# Install the required system libraries before running make.

ifeq ($(origin CC),default)
  CC := gcc
endif
PKG_CONFIG ?= pkg-config
RBDS ?= 0

# Detect native Windows and MinGW cross-compilers even when OS is not set.
TARGET_TRIPLE := $(shell $(CC) -dumpmachine 2>/dev/null)
ifneq ($(findstring mingw,$(TARGET_TRIPLE)),)
  WINDOWS := 1
else ifeq ($(OS),Windows_NT)
  WINDOWS := 1
else
  WINDOWS := 0
endif

ifeq ($(WINDOWS),1)
  BUILD_DIR := build/windows
  BIN := $(BUILD_DIR)/nanords.exe
  PACKAGES := samplerate portaudio-2.0
  PLATFORM_DEFS := -D_WIN32_WINNT=0x0601 -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS
  SYS_LIBS := -lshell32 -lm
else
  BUILD_DIR := build/linux
  BIN := $(BUILD_DIR)/nanords
  PACKAGES := samplerate ao
  PLATFORM_DEFS := -D_POSIX_C_SOURCE=200809L
  SYS_LIBS := -lm
endif

SRC := nanords.c ascii_cmd.c audio_output.c command_stream.c \
       control_input.c fm_mpx.c lib.c modulator.c osc.c \
       platform.c rds.c resampler.c waveforms.c
OBJS := $(addprefix $(BUILD_DIR)/,$(SRC:.c=.o))
DEPS := $(OBJS:.o=.d)

CPPFLAGS += $(PLATFORM_DEFS)
ifeq ($(RBDS),1)
  CPPFLAGS += -DRBDS
endif
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null)
CFLAGS += -O2 -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null) $(SYS_LIBS)

.PHONY: all clean check-deps help
all: $(BIN)

check-deps:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { echo "Missing pkg-config/pkgconf (PKG_CONFIG=$(PKG_CONFIG))."; exit 1; }
	@$(PKG_CONFIG) --exists $(PACKAGES) || { echo "Missing development libraries: $(PACKAGES)"; $(PKG_CONFIG) --print-errors --exists $(PACKAGES); exit 1; }

$(OBJS): | check-deps

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
	@echo "Built $@ (RDS subcarrier: fixed at 57000 Hz)"

clean:
	rm -rf build/

help:
	@echo "Build: make [RBDS=0|1] (RDS subcarrier fixed at 57 kHz)"
	@echo "Clean: make clean"
	@echo "Binary: $(BIN)"
	@echo "Libraries: $(PACKAGES) (via pkg-config)"

-include $(DEPS)
