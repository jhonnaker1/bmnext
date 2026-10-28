# BMNeXT

BMNeXT is a bare metal NeXT Computer, NeXTcube and NeXTstation emulator for
the Raspberry Pi 4 and 5. It runs the
[Previous](https://previous.alternative-system.com/) emulator directly on the
hardware, with no Linux underneath, the same way
[BMC64](https://github.com/randyrossi/bmc64) does for the Commodore 64,
[BMA800XL](https://github.com/aminch/bma800xl) for the Atari 800XL,
[BMCoco](https://github.com/jhonnaker1/bmcoco) for the Tandy Color Computer,
[BMApple2](https://github.com/jhonnaker1/bmapple2) for the Apple II and
[BMAtariST](https://github.com/jhonnaker1/bmatarist) for the Atari ST.

BMNeXT is based on BMAtariST, which is based on BMApple2, BMCoco,
[BMA800XL](https://github.com/aminch/bma800xl) by Adam Minchinton, and
[BMC64](https://github.com/randyrossi/bmc64) by Randy Rossi. The emulator is
replaced by Previous (itself based on Hatari); the kernel, display, sound,
network and web UI come from those projects.

BMNeXT wouldn't be possible without the work behind the open source
projects it builds on. Thanks to their authors and contributors for making
it available; see the [third-party](docs/THIRD_PARTY.md) acknowledgements.

**Status: early.** BMNeXT boots the NeXT's ROMs and NeXTSTEP 3.3 in the PC
simulator (`tools/hostsim`), and its kernels build, but it has not yet been
run on a Raspberry Pi, and its speed there is not known.

- Emulates the NeXT Computer (68030), NeXTcube, NeXTcube Turbo, NeXTstation,
  NeXTstation Turbo, NeXTstation Color and NeXTstation Turbo Color, chosen in
  the menu
- SCSI hard disk, CD-ROM and floppy images, optical (MO) disks and the floppy
  drive, as Previous has them
- The MegaPixel Display's 1120x832, in four greys or 4096 colours, pixel for
  pixel on a 1080p display
- USB keyboards (keys go to the NeXT by where they are) and mice
- The NeXT's own network (Machine, Ethernet Connected), through the Pi's
  Ethernet or Wi-Fi: NeXTSTEP gets its address by itself, and reaches what
  the Pi does
- No shutdown needed: just switch it off
- A menu (F12) for the disks (blank ones too), the machine, ROM, memory,
  what it starts from, the mouse's speed, video, sound and network
- Optional Ethernet or Wi-Fi with BMC64's web UI: manage the SD card from a
  browser and start disk images

Not emulated (yet): sound input, and the NeXTdimension board (the NeXTcube's 32-bit
colour graphics board, with its own Intel i860; planned for later). For
colour now, choose a NeXTstation Color or NeXTstation Turbo Color in the
menu (F12, Machine, Machine): 4096 colours on the same 1120x832 display.

## ROMs and NeXTSTEP

NeXT's ROMs and NeXTSTEP are copyrighted and not included. Copy your ROMs
into the card's `roms` folder: a 64K one for the NeXT Computer (ROM 1.x),
and 128K ones for the 68040 machines (ROM 2.x) and the Turbos (ROM 3.x).
Disk images go in `disks`. See [docs/ROMS.txt](docs/ROMS.txt).

## Supported hardware

The **Raspberry Pi 4 Model B and Pi 400** (`kernel8-rpi4.img`, 64-bit) and
the **Raspberry Pi 5, Pi 500 and Pi 500+** (`kernel_2712.img`, 64-bit). The
NeXT's 68030 and 68040 are emulated with their MMUs, which takes a fast
core. See [docs/PI4.md](docs/PI4.md) and [docs/PI5.md](docs/PI5.md).

## Getting started

1. Format an SD card as FAT32.
2. Copy the contents of a release zip (or of `build/sdcard/` after building)
   to the root of the card.
3. Copy your ROMs into `roms`, and a NeXTSTEP disk image into `disks`.
4. Boot the Pi with a USB keyboard and mouse connected. The NeXT tests
   itself and stops at its ROM monitor (`NeXT>`). Press **F12**, choose
   *Disks*, *SCSI Disk 0*, *Insert a Disk Image...*, and choose yours:
   the NeXT starts again, from it.

## Keys

| Key                      | Does                                              |
|--------------------------|---------------------------------------------------|
| F12                      | BMNeXT's menu                                     |
| Windows / Command        | Command                                           |
| Alt                      | Alternate                                         |
| F10 or Delete            | The NeXT's Power key                              |
| F5/F6 (End/Home)         | Sound volume down/up                              |
| F1/F2 (Page Down/Up)     | Brightness down/up                                |

See the [User Guide](docs/USER_GUIDE.md) for all the details, including the
options in `cmdline.txt`. All the documents are listed in
[docs/README.md](docs/README.md).

## Building

```
BMNEXT_ROMS=/path/to/your/roms ./build_sdcard.sh
```

builds the kernels for the Pi 4 and 5 (`./build_sdcard.sh pi5` builds one)
and stages the SD card files in `build/sdcard/`. It works on Linux and
macOS. The PC simulator's tests need your ROMs (`BMNEXT_ROMS`); without it
they are skipped. See [docs/BUILDING.md](docs/BUILDING.md).

BMNeXT can also run on a PC, in a simulator, for testing and debugging, and
live in a web page: see [docs/SIMULATOR.md](docs/SIMULATOR.md).

## How it fits together

```
src/main.cpp, kernel.cpp   Circle kernel: USB, GPIO, SD card; runs Previous on core 1
src/network.cpp, webui/    Ethernet / Wi-Fi and the web UI, from BMC64
src/host.h                 What the kernel offers the emulator
src/port/main_next.c       Previous's main loop, on one thread
src/port/platform.c        Previous's front end: starting it, ROMs, the display
src/port/host_next.c       Previous's host functions (locks, time) on one thread
src/port/screen.c          The NeXT's display to the frame buffer
src/port/audio.c           Previous's sound output
src/port/keymap.c          USB keys and mice to the NeXT's
src/port/nextbus.c         The NeXTbus, without the NeXTdimension
src/port/nonet.c           The network, unplugged
src/port/menu.c, osd.c     The menu
src/port/config.h          Previous's build options
src/port/previous.mk       Previous's (and zlib's) source list
third_party/previous       Previous (SVN trunk; tools/update_previous.sh)
third_party/zlib           zlib (submodule)
third_party/circle-stdlib  Circle and newlib (submodule)
src/av/, src/v3d/, src/kms Shared Pi 4/5 video, GPU and display drivers
tools/hostsim              Runs the emulator port on a PC for testing
```

Previous is used unmodified, minus the files BMNeXT must not ship (NeXT's
ROMs) or doesn't use yet (the NeXTdimension's i860 emulator): everything
specific to BMNeXT is in `src/`.
Previous's CPU core is generated when it is built, as in Previous's own
build.

## Licence

BMNeXT is free software under the GNU General Public License version 3;
see [LICENSE](LICENSE). It includes software from other projects under
their own licences (Previous under the GPL version 2 or later, with parts
from WinUAE, Hatari, ARAnyM and SoftFloat; zlib under its own); see
[docs/THIRD_PARTY.md](docs/THIRD_PARTY.md).
