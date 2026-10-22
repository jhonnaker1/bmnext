#
# Builds the PC test harness for the emulator port (see host_sim.c).
#
#   make -C tools/hostsim
#   tools/hostsim/run_tests.sh
#

ROOT := ../..
PREVIOUS := $(ROOT)/third_party/previous/src
ZLIB := $(ROOT)/third_party/zlib
include $(ROOT)/src/port/previous.mk

CC ?= cc
HOSTCC ?= cc
OUT := build
GEN := $(OUT)/gen

PREVIOUS_INCLUDES = -I $(ROOT)/src/port -I $(ROOT)/src/port/compat \
                    $(addprefix -I $(PREVIOUS)/,$(PREVIOUS_INCLUDE_DIRS)) \
                    -I $(PREVIOUS) -I $(GEN) -I $(ZLIB)
CFLAGS = -O2 -g -DBMNEXT_HOST_LIBC
PREVIOUS_FLAGS = $(CFLAGS) $(PREVIOUS_CFLAGS) $(PREVIOUS_INCLUDES) \
                 -include $(ROOT)/src/port/compat/previous_prefix.h

PORT_SRCS = platform.c menu.c osd.c posix_extra.c $(PREVIOUS_PORT_SRCS)

OBJS := $(addprefix $(OUT)/previous/,$(PREVIOUS_CORE_SRCS:.c=.o)) \
        $(addprefix $(OUT)/previous/cpu/,$(PREVIOUS_CPU_SRCS:.c=.o)) \
        $(addprefix $(OUT)/gen/,$(PREVIOUS_CPUGEN_SRCS:.c=.o) $(PREVIOUS_CPUDEFS_SRC:.c=.o)) \
        $(addprefix $(OUT)/previous/softfloat/,$(PREVIOUS_SOFTFLOAT_SRCS:.c=.o)) \
        $(addprefix $(OUT)/previous/dsp/,$(PREVIOUS_DSP_SRCS:.c=.o)) \
        $(addprefix $(OUT)/previous/debug/,$(PREVIOUS_DEBUG_SRCS:.c=.o)) \
        $(addprefix $(OUT)/zlib/,$(ZLIB_SRCS:.c=.o)) \
        $(addprefix $(OUT)/port/,$(PORT_SRCS:.c=.o)) \
        $(OUT)/host_sim.o $(OUT)/live.o

all: $(OUT)/bmnext_sim

$(OUT)/bmnext_sim: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

# The CPU core's generated sources, as Previous's build makes them
# (src/cpu/CMakeLists.txt).
$(GEN)/build68k: $(PREVIOUS)/cpu/build68k.c $(PREVIOUS)/cpu/writelog.c
	@mkdir -p $(GEN)
	$(HOSTCC) -I $(PREVIOUS)/cpu -I $(PREVIOUS)/includes -I $(ROOT)/src/port -o $@ $^

$(GEN)/cpudefs.c: $(GEN)/build68k $(PREVIOUS)/cpu/table68k
	$(GEN)/build68k < $(PREVIOUS)/cpu/table68k > $@

$(GEN)/gencpu: $(GEN)/cpudefs.c $(PREVIOUS)/cpu/gencpu.c $(PREVIOUS)/cpu/readcpu.c
	$(HOSTCC) -I $(PREVIOUS)/cpu -I $(PREVIOUS)/includes -I $(ROOT)/src/port -o $@ $^

# gencpu writes into the current folder.
$(GEN)/cpustbl.c: $(GEN)/gencpu
	cd $(GEN) && ./gencpu > /dev/null

$(addprefix $(GEN)/,$(filter-out cpustbl.c,$(PREVIOUS_CPUGEN_SRCS))) $(GEN)/cputbl.h: $(GEN)/cpustbl.c

$(OUT)/gen/%.o: $(GEN)/%.c
	@$(CC) $(PREVIOUS_FLAGS) -c -o $@ $<

$(OUT)/previous/%.o: $(PREVIOUS)/%.c $(GEN)/cputbl.h
	@mkdir -p $(dir $@)
	@$(CC) $(PREVIOUS_FLAGS) -c -o $@ $<

$(OUT)/zlib/%.o: $(ZLIB)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(ZLIB_CFLAGS) -c -o $@ $<

$(OUT)/port/%.o: $(ROOT)/src/port/%.c $(ROOT)/src/port/bmnext.h \
                 $(ROOT)/src/port/osd.h $(ROOT)/src/host.h $(GEN)/cputbl.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -std=gnu11 $(PREVIOUS_INCLUDES) -include $(ROOT)/src/port/compat/previous_prefix.h \
	      -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -c -o $@ $<

$(OUT)/host_sim.o: host_sim.c live.h $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/live.o: live.c live.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wextra -DLIVE_HTML=\"$(abspath live.html)\" -c -o $@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean
