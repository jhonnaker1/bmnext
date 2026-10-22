#
# Makefile for the BMNeXT kernel image.
#
# Run ./make_all.sh once first; it builds circle-stdlib and then this.
# After that "make" rebuilds the kernel image for the board circle-stdlib is
# built for: kernel7.img (Pi 2), kernel8-32.img (Pi 3), kernel8-rpi4.img
# (Pi 4, 64-bit) or kernel_2712.img (Pi 5, 64-bit).
# ./build_sdcard.sh stages everything for the SD card in build/sdcard.
#

ROOT := $(CURDIR)

ifeq ($(wildcard third_party/circle-stdlib/Config.mk),)
$(error circle-stdlib is not configured. Run ./make_all.sh first)
endif

include third_party/circle-stdlib/Config.mk
# RASPPI (the board) is in Circle's own Config.mk, which circle-stdlib's
# configure writes and Circle's Rules.mk reads; it's needed here first to
# pick the board's display and sound code.
include $(CIRCLEHOME)/Config.mk

BMNEXT_VERSION ?= $(shell tools/version.sh --semver)
BMNEXT_GIT ?= $(shell tools/version.sh --git)
PREVIOUS := third_party/previous/src
ZLIB := third_party/zlib
# "r1858": Previous's Subversion revision (tools/update_previous.sh).
PREVIOUS_VERSION := r$(shell cat third_party/previous/SVN_REVISION 2>/dev/null || echo unknown)

include src/port/previous.mk

BUILD := build
GEN := $(BUILD)/gen
PREVIOUS_OBJS := $(addprefix $(BUILD)/previous/,$(PREVIOUS_CORE_SRCS:.c=.o)) \
                 $(addprefix $(BUILD)/previous/cpu/,$(PREVIOUS_CPU_SRCS:.c=.o)) \
                 $(addprefix $(GEN)/,$(PREVIOUS_CPUGEN_SRCS:.c=.o) $(PREVIOUS_CPUDEFS_SRC:.c=.o)) \
                 $(addprefix $(BUILD)/previous/softfloat/,$(PREVIOUS_SOFTFLOAT_SRCS:.c=.o)) \
                 $(addprefix $(BUILD)/previous/dsp/,$(PREVIOUS_DSP_SRCS:.c=.o)) \
                 $(addprefix $(BUILD)/previous/debug/,$(PREVIOUS_DEBUG_SRCS:.c=.o)) \
                 $(addprefix $(BUILD)/zlib/,$(ZLIB_SRCS:.c=.o))
PREVIOUS_LIB := $(BUILD)/libprevious.a

OBJS = src/main.o src/kernel.o src/logging.o src/options.o \
       src/port/platform.o src/port/menu.o src/port/osd.o src/port/posix_extra.o \
       src/port/fswrap.o \
       $(addprefix src/port/,$(PREVIOUS_PORT_SRCS:.c=.o)) \
       src/network.o src/network_time_sync.o \
       src/webui/webui.o src/webui/webui_http.o src/webui/webui_fs.o \
       src/webui/webui_machine.o src/webui/webui_assets.o \
       src/sdcard/sd_fs_fatfs.o src/sdcard/sd_fs_file.o

# Display and sound. The Pi 2 and 3 use the VideoCore through VCHIQ
# (dispmanx, from BMC64); the Pi 4 and 5 use Circle's framebuffer and HDMI
# sound drivers (src/av/, shared by both; see docs/PI4.md and docs/PI5.md).
ifneq ($(filter 4 5,$(RASPPI)),)
OBJS += src/av/video.o src/av/sound.o src/av/hdmisound.o src/av/scaler.o \
        src/kms/common/filter.o src/kms/common/modes.o \
        src/v3d/gpu.o src/v3d/draw.o src/v3d/crt.o \
        src/v3d/shaders.o
# The direct display driver: the Pi 4's (src/kms/pi4/) or the Pi 5's
# (src/kms/pi5/), with shared filter kernels and mode calculations.
ifeq ($(RASPPI),4)
OBJS += src/kms/pi4/dlist.o src/kms/pi4/hvs.o src/kms/pi4/display.o \
        src/kms/pi4/phy.o src/kms/pi4/modeset.o
else
OBJS += src/kms/pi5/dlist.o src/kms/pi5/hvs.o src/kms/pi5/display.o \
        src/kms/pi5/phy.o src/kms/pi5/modeset.o
endif
# Smooth scaling and the CRT shader are drawn by the ARM: -O3 lets GCC turn
# their per-pixel loops into NEON code (tools/pi4scaler tests the same).
src/av/scaler.o: OPTIMIZE = -O3
else
OBJS += src/fbl.o src/crt_pi_idx.o src/crt_pi_rgb.o src/vchiqsound.o \
        src/vchiqsoundbasedevice.o
endif

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(CIRCLEHOME)/addon/fatfs

# See src/port/fswrap.c
LDFLAGS += --wrap=stat

VERSION_DEFINES = -DBMNEXT_VERSION=\"$(BMNEXT_VERSION)\" \
                  -DBMNEXT_GIT=\"$(BMNEXT_GIT)\" -DPREVIOUS_SVN=\"$(PREVIOUS_VERSION)\"
CFLAGS += $(VERSION_DEFINES)
CPPFLAGS += $(VERSION_DEFINES)

PREVIOUS_INCLUDES = -I src/port -I src/port/compat \
                    $(addprefix -I $(PREVIOUS)/,$(PREVIOUS_INCLUDE_DIRS)) \
                    -I $(PREVIOUS) -I $(GEN) -I $(ZLIB)

# The port's C files include Previous's headers.
src/port/%.o src/port/%.d: CFLAGS += $(PREVIOUS_INCLUDES) -std=gnu11 \
	-include src/port/compat/previous_prefix.h
$(addprefix src/port/,$(PREVIOUS_PORT_SRCS:.c=.o) platform.o menu.o): $(GEN)/cputbl.h

# Previous itself (and zlib): compiled into build/ so third_party stays
# clean.
PREVIOUS_FLAGS = $(PREVIOUS_CFLAGS) $(PREVIOUS_INCLUDES) -include src/port/compat/previous_prefix.h

# The CPU core's generated sources, as Previous's build makes them for a
# cross compile (src/cpu/CMakeLists.txt): build68k and gencpu are built for
# this computer, with its compiler.
HOSTCC ?= cc

$(GEN)/build68k: $(PREVIOUS)/cpu/build68k.c $(PREVIOUS)/cpu/writelog.c
	@mkdir -p $(GEN)
	@echo "  HOSTCC $@"
	@$(HOSTCC) -I $(PREVIOUS)/cpu -I $(PREVIOUS)/includes -I src/port -o $@ $^

$(GEN)/cpudefs.c: $(GEN)/build68k $(PREVIOUS)/cpu/table68k
	@echo "  GEN   $@"
	@$(GEN)/build68k < $(PREVIOUS)/cpu/table68k > $@

$(GEN)/gencpu: $(GEN)/cpudefs.c $(PREVIOUS)/cpu/gencpu.c $(PREVIOUS)/cpu/readcpu.c
	@echo "  HOSTCC $@"
	@$(HOSTCC) -I $(PREVIOUS)/cpu -I $(PREVIOUS)/includes -I src/port -o $@ $^

$(GEN)/cpustbl.c: $(GEN)/gencpu
	@echo "  GEN   $(GEN)/cpuemu_*.c"
	@cd $(GEN) && ./gencpu > /dev/null

$(addprefix $(GEN)/,$(filter-out cpustbl.c,$(PREVIOUS_CPUGEN_SRCS))) $(GEN)/cputbl.h: $(GEN)/cpustbl.c

# A static pattern rule: Circle's own %.o: %.c (Rules.mk) would otherwise be
# used for these, which are .c files next to their .o (make 3.81, macOS's,
# takes the first rule that matches).
PREVIOUS_GEN_OBJS := $(addprefix $(GEN)/,$(PREVIOUS_CPUGEN_SRCS:.c=.o) $(PREVIOUS_CPUDEFS_SRC:.c=.o))
$(PREVIOUS_GEN_OBJS): $(GEN)/%.o: $(GEN)/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(PREVIOUS_FLAGS) -c -o $@ $<

$(BUILD)/previous/%.o: $(PREVIOUS)/%.c $(GEN)/cputbl.h
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(PREVIOUS_FLAGS) -c -o $@ $<

$(BUILD)/zlib/%.o: $(ZLIB)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(ZLIB_CFLAGS) -c -o $@ $<

$(PREVIOUS_LIB): $(PREVIOUS_OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(PREVIOUS_OBJS)

# The web UI's pages are compiled in. Regenerate them when they change.
WEBUI_ASSETS = $(shell find src/webui/assets -type f)
src/webui/webui_assets.c: $(WEBUI_ASSETS) tools/gen_webui_assets.py
	@echo "  GEN   $@"
	@python3 tools/gen_webui_assets.py

$(TARGET).img: $(PREVIOUS_LIB)

ifneq ($(filter 4 5,$(RASPPI)),)
VIDEOCORE_LIBS :=
else
VIDEOCORE_LIBS := \
	$(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	$(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
	$(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
	$(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

LIBS := $(PREVIOUS_LIB) \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libm.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libc.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libcirclenewlib.a \
	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/net/libnet.a \
	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
	$(CIRCLEHOME)/addon/wlan/libwlan.a \
	$(VIDEOCORE_LIBS) \
	$(CIRCLEHOME)/lib/sound/libsound.a \
	$(CIRCLEHOME)/lib/sched/libsched.a \
	$(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN += $(OBJS) src/*.o src/port/*.d src/*.d src/webui/*.d src/sdcard/*.d \
              src/av/*.o src/av/*.d src/kms/common/*.o src/kms/common/*.d \
              src/kms/pi4/*.o src/kms/pi4/*.d \
              src/kms/pi5/*.o src/kms/pi5/*.d src/v3d/*.o src/v3d/*.d
.PHONY: distclean

distclean: clean
	rm -rf $(BUILD)

-include $(DEPS)
