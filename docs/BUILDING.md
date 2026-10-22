# Build Instructions

BMNeXT builds on Linux (Debian, Ubuntu, WSL) and macOS.

## Prerequisites

On Debian or Ubuntu:

```
sudo apt-get install build-essential git python3 curl xz-utils
```

On macOS (Apple silicon or Intel), with the Xcode command line tools and
[Homebrew](https://brew.sh):

```
xcode-select --install
brew install bash gnu-getopt
```

circle-stdlib's `configure` needs bash 4 or later and GNU `getopt`, which
`make_all.sh` puts first on the PATH on macOS. newlib's build would like
`makeinfo` (texinfo) for its manuals and Circle's Wi-Fi firmware download
uses `wget`; without them, `make_all.sh` and `make_wifi_firmware.sh` use
stand-ins (the manuals aren't needed, and `curl` does the download).

The Arm GNU Toolchain 15.2.rel1 is downloaded into
`~/.cache/bmnext/toolchains` the first time a build needs it
(`get_gnu_toolchain.sh`), built for your computer (Linux or macOS):
`aarch64-none-elf` for the 64-bit Pi 4 and 5 kernels (and `arm-none-eabi`
for the family's 32-bit Pi Zero, 2 and 3 kernels, which BMNeXT doesn't
need). If it can't be downloaded,
`BMNEXT_ALLOW_ANY_TOOLCHAIN=1` lets the build use whatever
`arm-none-eabi-gcc` (or `aarch64-none-elf-gcc`) is on the PATH.

## Downloading the files

```
git clone --recursive https://github.com/jhonnaker1/bmnext.git
```

or, in an existing clone, `git submodule update --init --recursive`.
Previous itself is in the repository (`third_party/previous`), not a
submodule: it is kept in Subversion (see [UPDATING.md](UPDATING.md)).

## Building an SD card

```
BMNEXT_ROMS=/path/to/your/roms ./build_sdcard.sh
```

builds everything from clean and stages the boot partition in
`build/sdcard`, with a kernel for each board: `kernel8-rpi4.img` (with
`armstub8-rpi4.bin`) for the Pi 4 B and Pi 400, and `kernel_2712.img` for
the Pi 5, Pi 500 and Pi 500+. `./build_sdcard.sh pi4` or
`./build_sdcard.sh pi5` builds just one. Copy the contents of that folder
to the root of a FAT32 formatted SD card, and your ROMs into its `roms`
folder.

The PC tests (`tools/hostsim`, `tools/pi4kms` and `tools/pi4scaler`, below)
run first, and the build stops if any of them fails; `--skip-tests` leaves
them out. `tools/hostsim`'s need NeXT ROMs, which are not part of BMNeXT:
`BMNEXT_ROMS` is a folder with yours, and without it they are left out.

While developing, `./build_sdcard.sh --skip-patches` keeps the third party
libraries already built and only rebuilds the kernel, which takes seconds.
The libraries are built for one board at a time, so without a board it
rebuilds the kernel for the board built last; a board they aren't built for
is built from clean. With
[networking](USER_GUIDE.md#networking-and-the-web-ui) on, a new kernel can
then be uploaded to the Pi from the web UI and the Pi
rebooted from there, without touching the SD card.

## The individual steps

1. `./clean_all.sh` cleans everything, including the third party builds,
   and resets Circle to its pinned revision.
2. `./make_all.sh pi4|pi5 [--skip-patches]` applies `src/patches/` to
   Circle, builds circle-stdlib (Circle and newlib) and the Circle addons
   for one board, then the kernel: `kernel8-rpi4.img` (64-bit) for pi4,
   which also builds Circle's `armstub8-rpi4.bin`, or `kernel_2712.img`
   (64-bit) for pi5. (pi0, pi2 and pi3 still build, as in the rest of the
   family, but are too slow for the NeXT.) It only needs running again after
   `clean_all.sh` or a change to the patches or circle-stdlib. To change
   board, run `clean_all.sh` first (make_all.sh refuses otherwise).
3. `BMNEXT_AARCH=64 source get_gnu_toolchain.sh && make` rebuilds just the
   kernel.
4. `./make_wifi_firmware.sh DIR` puts the Wi-Fi firmware in `DIR/firmware`
   (`build_sdcard.sh` does this).

`config.txt` has a section per board that picks its kernel. The emulator
runs on core 1. See [PI4.md](PI4.md) and [PI5.md](PI5.md) for the Pi 4
and 5 builds.

## Testing on a PC

`tools/hostsim` builds the emulator port (Previous plus `src/port/`) for
the PC, with a simulated display, sound and input in place of the
Raspberry Pi. It runs scripted tests with your ROMs, starting the NeXT
Computer, the NeXTstation and the NeXTstation Turbo Color to their ROM
monitors: typing at the monitor, the menu, a CPU that halts, a card with no
ROM, web UI requests and the power key.

```
BMNEXT_ROMS=/path/to/your/roms tools/hostsim/run_tests.sh
```

Screenshots (`.ppm`) are written to `tools/hostsim/out/`.

To play with the port on the PC, run it live:

```
tools/hostsim/live.sh /path/to/your/roms
```

and open http://127.0.0.1:8064/ in a browser: the screen, keyboard, mouse
and sound are in the page. The simulator's SD card is
`tools/hostsim/out/live`, set up as the Pi's. This runs BMNeXT's port and
Previous on the PC, not the Pi kernel (no emulator
shows the Pi's VideoCore display). The web page is served on 127.0.0.1
only.

The simulator can also be driven by a script of key presses, mouse moves
and screenshots; see the top of `tools/hostsim/host_sim.c` for the
commands. [SIMULATOR.md](SIMULATOR.md) has more: setting it up, debugging
it with gdb, lldb or strace, and getting a log from the Pi.

`tools/pi4scaler` tests the shared Pi 4/5 ARM smooth scaling and CRT shader
(`src/av/scaler.*`): every pixel is checked against crt-pi worked out in
floating point, and the pictures are written to `tools/pi4scaler/out/`.
`tools/pi4kms` tests the Pi 4/5 direct display drivers (`src/kms/`, see
[PI4.md](PI4.md) and [PI5.md](PI5.md)): shared filter kernels, EDID, mode
choice, HDMI packets and audio clocks, and each board's display lists and
PHY settings.
Both come from BMA800XL.

```
tools/pi4scaler/run_tests.sh
tools/pi4kms/run_tests.sh
```

## The web UI

The web UI's pages are in `src/webui/assets/`. They are compiled into the
kernel through `src/webui/webui_assets.c`, which the Makefile regenerates
with `tools/gen_webui_assets.py` whenever an asset changes.

## Layout

| Path                         | What                                        |
|------------------------------|---------------------------------------------|
| `src/main.cpp`, `kernel.*`   | Circle kernel; USB, GPIO, SD card, cores    |
| `src/network.*`              | Ethernet / Wi-Fi start up (from BMC64)      |
| `src/network_time_sync.*`    | Clock from the network (from BMC64)         |
| `src/webui/`                 | Web UI server, file manager and pages (from BMC64) |
| `src/sdcard/`                | SD card helpers used by the web UI (from BMC64) |
| `src/fbl.*`, `crt_pi_*`      | Display layer and CRT shaders (from BMC64)  |
| `src/vchiqsound*`            | Sound through the VideoCore (from BMC64)    |
| `src/options.*`              | `cmdline.txt` options                       |
| `src/host.h`                 | Interface between kernel and emulator       |
| `src/port/main_next.c`       | Previous's main loop on one thread: start up, the CPU, its 200 Hz event handler |
| `src/port/platform.c`        | Previous's front end: starting it, ROMs, showing the display, web UI requests |
| `src/port/host_next.c`       | Previous's host functions (locks, atomics, time) on one thread |
| `src/port/screen.c`          | The NeXT's display to the frame buffer (for Previous's src/gui-sdl/sdlscreen.c) |
| `src/port/audio.c`           | Previous's sound output |
| `src/port/keymap.c`          | USB keys and mice to the NeXT's (kms.c) or ADB keyboard's codes |
| `src/port/gui_event.c`       | Keyboard, mouse and gamepads into Previous  |
| `src/port/nextbus.c`         | The NeXTbus without the NeXTdimension       |
| `src/port/nonet.c`           | The NeXT's network, unplugged               |
| `src/port/dialogs.c`         | What Previous asks its dialogs              |
| `src/port/statusbar.c`       | The disk activity light                     |
| `src/port/menu.c`, `osd.c`   | The menu and its on-screen display          |
| `src/port/fswrap.c`          | `stat()` of the SD card's root, which FatFs can't do (`--wrap=stat`) |
| `src/port/posix_extra.c`     | timegm(), scandir(), utime(), access(), chmod() for newlib |
| `src/port/compat/`           | Included before Previous's files, and the SDL types its host.h names |
| `src/port/config.h`          | Previous's build options                    |
| `src/port/previous.mk`       | Which Previous and zlib sources are built   |
| `src/patches/`               | Changes applied to Circle by `make_all.sh`  |
| `sdcard/`                    | Files for the SD card                       |
| `third_party/previous`       | Previous (from its Subversion trunk; `tools/update_previous.sh`) |
| `third_party/zlib`           | zlib (submodule)                            |
| `third_party/circle-stdlib`  | Circle and newlib (submodule)               |
| `src/av/`                    | Shared Pi 4/5 video, ARM scaling / CRT shader and sound |
| `src/v3d/`                   | Shared Pi 4/5 GPU driver and CRT rendering  |
| `src/kms/common/`            | Shared Pi 4/5 filter kernels and mode calculations |
| `src/kms/pi4/`               | Pi 4 direct display driver and PHY          |
| `src/kms/pi5/`               | Pi 5 direct display driver and PHY          |
| `tools/hostsim`              | PC test harness                             |
| `tools/pi4scaler`            | PC test of shared Pi 4/5 ARM scaling and CRT rendering |
| `tools/pi4kms`               | PC test of Pi 4/5 display driver calculations |
