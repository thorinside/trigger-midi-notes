# Makefile for Trigger MIDI Notes distingNT plugin
#
# Dual build targets:
#   make hardware - Build ARM .o for disting NT hardware
#   make test     - Build native .dylib/.so for desktop testing in VCV Rack nt_emu
#   make push     - Build and push to disting NT hardware via ntpush
#   make clean    - Clean build artifacts

PROJECT = trigger_midi_notes
DISTINGNT_API = distingNT_API

# Source files
SOURCES = trigger_midi_notes.cpp

# Include paths
INCLUDES = \
	-I$(DISTINGNT_API)/include

# Common flags
CXXFLAGS_COMMON = -std=c++11 -Wall -Wextra -Werror -fno-rtti -fno-exceptions

# Hardware target (ARM Cortex-M7)
CXX_ARM = arm-none-eabi-g++
CXXFLAGS_ARM = $(CXXFLAGS_COMMON) \
	-mcpu=cortex-m7 \
	-mfpu=fpv5-d16 \
	-mfloat-abi=hard \
	-mthumb \
	-Os \
	-fPIC \
	-fdata-sections \
	-ffunction-sections

# Desktop test target
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CXX_TEST = clang++
    DYLIB_EXT = dylib
else
    CXX_TEST = g++
    DYLIB_EXT = so
endif

CXXFLAGS_TEST = $(CXXFLAGS_COMMON) -O2 -fPIC -DTEST

# Output directories
PLUGINS_DIR = plugins
BUILD_DIR = build

# Targets
.PHONY: all hardware test clean push

all: hardware test

# Hardware build
hardware: $(PLUGINS_DIR)/$(PROJECT).o

$(BUILD_DIR)/$(PROJECT).o: $(SOURCES) | $(BUILD_DIR)
	$(CXX_ARM) $(CXXFLAGS_ARM) $(INCLUDES) -c $< -o $@

$(PLUGINS_DIR)/$(PROJECT).o: $(BUILD_DIR)/$(PROJECT).o | $(PLUGINS_DIR)
	$(CXX_ARM) -r $< -o $@
	@echo "Hardware build complete: $@"
	@ls -lh $@

# Test build - single-shot compile to .dylib/.so
test: $(PLUGINS_DIR)/$(PROJECT).$(DYLIB_EXT)

$(PLUGINS_DIR)/$(PROJECT).$(DYLIB_EXT): $(SOURCES) | $(PLUGINS_DIR)
ifeq ($(UNAME_S),Darwin)
	$(CXX_TEST) $(CXXFLAGS_TEST) $(INCLUDES) -dynamiclib -undefined dynamic_lookup $(SOURCES) -o $@
else
	$(CXX_TEST) $(CXXFLAGS_TEST) $(INCLUDES) -shared $(SOURCES) -o $@
endif
	@echo "Desktop test build complete: $@"
	@ls -lh $@

$(PLUGINS_DIR):
	mkdir -p $(PLUGINS_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

push: hardware
	ntpush $(PLUGINS_DIR)/$(PROJECT).o

clean:
	rm -rf $(PLUGINS_DIR) $(BUILD_DIR)
	@echo "Build artifacts cleaned"
