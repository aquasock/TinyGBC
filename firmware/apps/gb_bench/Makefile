# SPDX-License-Identifier: MIT
#
# Game Boy Color throughput benchmark: gambatte's libretro core, fetched by
# tools/fetch_gambatte.sh, driven by main.cpp as the frontend, with a
# cartridge image built in.
#
#   make ROM=<file.gbc>         AE350 program, build/firmware/apps/gb_bench/
#   make ROM=<file.gbc> host    the same program for this PC, in host/
#   make ROM=<file.gbc> PLAY=1 host
#                               a playable window on this PC, for taking a
#                               savestate by hand: X11, keys as printed at
#                               start, F5 saves
#
# FRAMES=<n>, INPUT="<script>" and CHECK=<n> replace the defaults in main.cpp.
# DISPLAY=1 writes each frame into the video unit's frame buffer and hands it
# over at a vertical blank; PACE=1 also runs one emulated frame per Game Boy
# frame, so the picture can be watched. PACE implies DISPLAY. AUDIO=1 resamples
# the core's 32,768 Hz sound to the DAC's 48,828.125 Hz and plays it on the
# Pmod I2S2 and HDMI; the PC build has no sink and only checksums the samples.
# The programs contain the core and are GPL-2.0
# (third_party/gambatte-libretro/COPYING); with the cartridge image built in
# they are for local use only.

APP     := gb_bench
SOURCES := main.cpp rom.S state.S

GAMBATTE_DIR := $(abspath ../../../third_party/gambatte-libretro)
ifeq ($(wildcard $(GAMBATTE_DIR)/Makefile.common),)
$(error gambatte not found in $(GAMBATTE_DIR): run tools/fetch_gambatte.sh)
endif

# Upstream's unix build, without the network serial link (HAVE_NETWORK stays
# 0, so net_serial.cpp is not built) and with RGB565 output, the format the
# A25 writes fastest.
CORE_DIR     := $(GAMBATTE_DIR)/libgambatte/src
HAVE_NETWORK := 0
include $(GAMBATTE_DIR)/Makefile.common

LIB_SOURCES  := $(SOURCES_C) $(SOURCES_CXX)
LIB_CPPFLAGS := $(INCFLAGS) -D__LIBRETRO__ -DHAVE_STDINT_H -DHAVE_INTTYPES_H \
                -DCC_RESAMPLER_NO_HIGHPASS -DVIDEO_RGB565 -DHAVE_NO_LANGEXTRA \
                -DNDEBUG
LIB_CXXFLAGS := -std=gnu++11 -fno-rtti -fno-exceptions

# main.cpp includes the core's libretro.h and its own headers.
CPPFLAGS += $(INCFLAGS)

ifeq ($(PLAY),1)
APP_DEFS += -DGB_PLAY
BUILD     ?= $(abspath ../../../build/firmware/apps/gb_play)
endif
APP_DEFS += $(if $(FRAMES),-DGB_FRAMES=$(FRAMES)) \
            $(if $(INPUT),-DGB_INPUT='"$(INPUT)"') \
            $(if $(CHECK),-DGB_CHECK=$(CHECK))
ifneq ($(filter 1,$(DISPLAY) $(PACE)),)
APP_DEFS += -DGB_DISPLAY
endif
ifeq ($(PACE),1)
APP_DEFS += -DGB_PACE
endif
ifeq ($(AUDIO),1)
APP_DEFS += -DGB_AUDIO
endif
ifeq ($(PAD),1)
APP_DEFS += -DGB_PAD
endif
ifeq ($(PADTRACE),1)
APP_DEFS += -DGB_PAD -DGB_PAD_TRACE
endif
ifeq ($(LBTEST),1)
APP_DEFS += -DGB_LB_TEST
endif
CPPFLAGS += $(APP_DEFS)

include ../../rt/rt.mk

# FRAMES, INPUT and CHECK are defines, so a change to them alone would leave
# main.cpp's object stale: the options are kept in a file that is rewritten
# only when they change, and both builds of main.cpp depend on it.
DEFS_STAMP := $(BUILD)/app_defs
ifneq ($(file <$(DEFS_STAMP)),$(APP_DEFS))
$(shell mkdir -p $(BUILD))
$(file >$(DEFS_STAMP),$(APP_DEFS))
endif
$(call objects,main.cpp): $(DEFS_STAMP)

ASFLAGS += -DGB_ROM_FILE='"$(BUILD)/rom.gbc"'
$(call objects,rom.S): $(BUILD)/rom.gbc

# The savestate, embedded the same way; an empty file when STATE is unset.
ASFLAGS += -DGB_STATE_FILE='"$(BUILD)/gb.state"'
$(call objects,state.S): $(BUILD)/gb.state
$(BUILD)/gb.state: FORCE
	@mkdir -p $(dir $@)
	@if [ -n "$(STATE)" ]; then cmp -s "$(STATE)" $@ || cp "$(STATE)" $@; \
	 else : > $@; fi

# Copied so that the image path may hold spaces, and only when it changes.
$(BUILD)/rom.gbc: FORCE
	@test -n "$(ROM)" || { echo "set ROM=<file.gbc>" >&2; exit 1; }
	@mkdir -p $(dir $@)
	@cmp -s "$(ROM)" $@ || cp "$(ROM)" $@

# The PC build: the same sources with the host compiler.
HOST     := $(BUILD)/host
HOST_CC  ?= gcc
HOST_CXX ?= g++
host_object = $(patsubst $(ROOT)/%,$(HOST)/obj/%.o,$(abspath $(1)))
HOST_LIB_OBJECTS := $(foreach s,$(LIB_SOURCES),$(call host_object,$(s)))
HOST_APP_OBJECTS := $(foreach s,$(SOURCES),$(call host_object,$(s)))

host: $(HOST)/$(APP)

HOST_LIBS := $(if $(filter 1,$(PLAY)),-lX11)
$(HOST)/$(APP): $(HOST_APP_OBJECTS) $(HOST_LIB_OBJECTS)
	$(HOST_CXX) -o $@ $^ $(HOST_LIBS)

define host_compile
$(call host_object,$(1)): $(1) $(MAKEFILE_LIST)
	@mkdir -p $$(dir $$@)
	$(2) -c -o $$@ $(1)
endef
$(foreach s,$(filter %.c,$(LIB_SOURCES)),$(eval $(call host_compile,$(s),$$(HOST_CC) -MMD -MP $$(LIB_CPPFLAGS) -O2 $$(LIB_CFLAGS))))
$(foreach s,$(filter %.cpp,$(LIB_SOURCES)),$(eval $(call host_compile,$(s),$$(HOST_CXX) -MMD -MP $$(LIB_CPPFLAGS) -O2 $$(LIB_CXXFLAGS))))
$(eval $(call host_compile,main.cpp,$$(HOST_CXX) -MMD -MP $$(CPPFLAGS) -O2 -std=gnu++20 -Wall -Wextra -Werror))
$(eval $(call host_compile,rom.S,$$(HOST_CC) $$(APP_DEFS) -DGB_ROM_FILE='"$$(BUILD)/rom.gbc"'))
$(call host_object,rom.S): $(BUILD)/rom.gbc
$(eval $(call host_compile,state.S,$$(HOST_CC) $$(APP_DEFS) -DGB_STATE_FILE='"$$(BUILD)/gb.state"'))
$(call host_object,state.S): $(BUILD)/gb.state
$(call host_object,main.cpp): $(DEFS_STAMP)

-include $(HOST_LIB_OBJECTS:.o=.d) $(HOST_APP_OBJECTS:.o=.d)

.PHONY: host FORCE
