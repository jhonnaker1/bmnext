# Third-Party Software

BMNeXT incorporates software and other materials from third-party projects. Third-party components remain subject to their respective copyright and license terms.

The original copyright, license, and attribution notices supplied with these components are retained with their source where applicable. The notices and licences for code BMNeXT adapts from other projects are in [`sdcard/licenses/`](../sdcard/licenses/), which becomes the `licenses` folder of the SD card.

## Previous

BMNeXT incorporates [Previous](https://previous.alternative-system.com/), the NeXT emulator by the Previous team (Andreas Grabher, Simon Schubiger and Gilles Fétis), based on the Atari ST emulator Hatari, under the GNU General Public License version 2 or (at your option) any later version. The version used is its Subversion trunk at the revision in `third_party/previous/SVN_REVISION` (r1858, Previous 4.5), from `https://svn.code.sf.net/p/previous/code/trunk`; Previous has no official git repository, so it is kept in this repository as files, not as a submodule (`tools/update_previous.sh`).

Previous's 68030 and 68040 CPU core comes from WinUAE and UAE; its DSP56001 emulation from ARAnyM and Hatari (GPL version 2 or later); its floating point emulation from QEMU and the SoftFloat package (under its own notice, in `src/softfloat/`). BMNeXT's menu uses the character set from Previous's font (`src/gui-sdl/font10x16.h`). BMNeXT's front end (`src/port/`) has the same functions as Previous's SDL front end (`src/gui-sdl/`).

Left out of `third_party/previous` (and so of BMNeXT):

- NeXT's ROM images and a ROM disassembly, which Previous's trunk includes (`src/*.BIN`, `src/ROMV66-0001E-02588.ASM`): they are NeXT's (now Apple's) copyright. Users supply their own ROMs.
- The NeXTdimension's Intel i860 emulator (`src/dimension/i860*`), by Jason Eckhardt. Previous's copy still carries the old MAME licence (non-commercial use), which is not compatible with the GPL. MAME's own copy of the same emulator has been under the BSD 3-clause licence (GPL compatible) since MAME's relicensing in 2016, so the NeXTdimension can come later, once that is settled (see [UPDATING.md](UPDATING.md)). The rest of `src/dimension/` is there but not built.
- Blank disk images (`disks/`).

Previous's SLiRP network code (`src/slirp/`, BSD licence) is in the tree but not built.

Its notice is in [`sdcard/licenses/previous.txt`](../sdcard/licenses/previous.txt) and the GNU General Public License version 2 in [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt).

## zlib

BMNeXT incorporates [zlib](https://zlib.net/) 1.3.2 (the `third_party/zlib` submodule), for gzipped disk images, under the zlib licence. Its notice is in [`sdcard/licenses/zlib.txt`](../sdcard/licenses/zlib.txt).

## BMAtariST

BMNeXT is based on [BMAtariST](https://github.com/jhonnaker1/bmatarist), the bare metal Atari ST emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network, web UI, menu and test harness code is used here.

## BMApple2

BMAtariST, and so BMNeXT, is based on [BMApple2](https://github.com/jhonnaker1/bmapple2), the bare metal Apple II emulator for the Raspberry Pi, under the GNU General Public License version 3.

## BMCoco

BMApple2, and so BMNeXT, is based on [BMCoco](https://github.com/jhonnaker1/bmcoco), the bare metal Tandy Color Computer emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network, web UI, menu and test harness code is used here.

## BMA800XL

BMCoco, and so BMNeXT, is a fork of [BMA800XL](https://github.com/aminch/bma800xl), the bare metal Atari 800XL emulator for the Raspberry Pi, under the GNU General Public License version 3. Its kernel, display, sound, network and web UI code is used here, including its Raspberry Pi 4 and Pi 5 display, sound and GPU drivers, merged up to BMA800XL 1.4.2.

## circle-stdlib

BMNeXT uses the [circle-stdlib project](https://github.com/smuehlst/circle-stdlib), v21, which includes [Circle](https://github.com/rsta2/circle) by Rene Stange and the newlib C library.

See [`third_party/circle-stdlib/`](https://github.com/smuehlst/circle-stdlib/tree/v21) for the version used by BMNeXT and its accompanying copyright and license information. newlib's notices are in [`sdcard/licenses/newlib.txt`](../sdcard/licenses/newlib.txt) and [`sdcard/licenses/libgloss.txt`](../sdcard/licenses/libgloss.txt).

## GCC Runtime

BMNeXT links runtime code from the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 15.2.rel1 (GCC), under the GNU General Public License version 3 with the GCC Runtime Library Exception 3.1. The toolchain's GCC licence section is in [`sdcard/licenses/GCC-Runtime.txt`](../sdcard/licenses/GCC-Runtime.txt).

## BMC64

BMNeXT incorporates source code from the [BMC64 project](https://github.com/randyrossi/bmc64) by Randy Rossi and contributors.

See [BMC64](https://github.com/randyrossi/bmc64) for its source and its accompanying copyright and license information.

BMC64-derived files retain their original license and attribution notices. For files licensed under Apache-2.0, see [the included license](../sdcard/licenses/Apache-2.0.txt); modifications are identified in their headers.

## Raspberry Pi Firmware

BMNeXT includes the Raspberry Pi boot firmware (`bootcode.bin`, `start.elf`, `fixup.dat`, and for the Raspberry Pi 4 `start4.elf` and `fixup4.dat`) and the Raspberry Pi 4 and Pi 5 device trees from the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware).

Their licences are next to them, in [`sdcard/firmware/`](../sdcard/firmware/) and on the SD card: `LICENCE.broadcom` for the firmware and `COPYING.linux` for the device trees. See the [Raspberry Pi firmware repository](https://github.com/raspberrypi/firmware/tree/master/boot) for more.

## crt-pi

BMNeXT uses the crt-pi CRT shader by davej, as included in BMC64, under the GNU General Public License version 2 or later: on the ARM, and compiled for the Raspberry Pi 4 and Pi 5's 3D GPU from [`tools/v3dshader/crt.frag`](../tools/v3dshader/crt.frag). See [`sdcard/licenses/crt-pi.txt`](../sdcard/licenses/crt-pi.txt) and [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt).

## Mesa

BMNeXT uses the [Mesa 3D graphics library](https://mesa3d.org/)'s Broadcom V3D drivers in three ways. The adapted files, with their copyright notices, and Mesa's MIT licence are in [`sdcard/licenses/Mesa-MIT.txt`](../sdcard/licenses/Mesa-MIT.txt).

- **Adapted into the Raspberry Pi 4 and Pi 5 kernels** ([`src/v3d/`](../src/v3d/)): control list packet layouts and packing rules, the rendering job, texture and tile layouts, and the GPU's device facts. The V3D 4.2 (Pi 4) material is from Mesa 26.2.4, the V3D 7.1 (Pi 5) material from Mesa 25.2.8.
- **Adapted into the shader compiler tool** ([`tools/v3dshader/`](../tools/v3dshader/)): the shader preparation that Mesa's drivers do before compiling. The tool is built on a PC against Mesa 25.2.8 and is not part of the SD card.
- **As the shader compiler:** the compiled shaders in `src/v3d/` (`shader_*.h`, `shader71_*.h`) are output of Mesa 25.2.8's V3D compiler. Compiling with Mesa doesn't make them Mesa code: the crt-pi shaders are under crt-pi's licence (above), the others are BMNeXT's.

## Linux

For the 3D GPU of the Raspberry Pi 4 and Pi 5, BMNeXT adapts parts of the Linux kernel's V3D driver and BCM2835 power domain driver ([`src/v3d/`](../src/v3d/)), from the [Raspberry Pi kernel](https://github.com/raspberrypi/linux/tree/rpi-6.18.y) (`rpi-6.18.y`), under the GNU General Public License version 2 or later. See [`sdcard/licenses/Linux-V3D.txt`](../sdcard/licenses/Linux-V3D.txt) and [`sdcard/licenses/GPL-2.0.txt`](../sdcard/licenses/GPL-2.0.txt).

The Raspberry Pi 4 and Pi 5 display implementation ([`src/kms/pi4/`](../src/kms/pi4/), [`src/kms/pi5/`](../src/kms/pi5/) and the HDMI sound in [`src/av/`](../src/av/)) was independently written using hardware information documented in the Linux vc4 driver (`rpi-6.18.y`). No code from that driver was copied or incorporated.

## Circle ARM stub

For the Raspberry Pi 4, BMNeXT includes `armstub8-rpi4.bin`, built from [Circle's `boot/armstub`](https://github.com/rsta2/circle/tree/master/boot/armstub) source, under Circle's [GNU General Public License version 3](https://github.com/rsta2/circle/blob/master/COPYING).

## Wi-Fi Firmware

BMNeXT includes the Wi-Fi firmware for the Raspberry Pi's wireless chip from the [RPi-Distro firmware-nonfree repository](https://github.com/RPi-Distro/firmware-nonfree), downloaded when BMNeXT is built.

See the `firmware/` folder on the SD card for the Broadcom, Cypress and Synaptics licenses that accompany it.

The Pi 5's files (`brcmfmac43455-sdio.raspberrypi,5-model-b.*`) are copies of the `brcmfmac43455-sdio.*` files under the name Circle looks for on the Pi 5, under the same Cypress license.

## BMRetroSys

[BMRetroSys](https://github.com/drunkeneye/BMRetroSys), a bare metal emulator collection for the Raspberry Pi 4 and 5 that also runs Hatari, was a reference (through BMAtariST) for the pitfalls of running a Hatari-style emulator on Circle (restarting the CPU core after a reset, what newlib lacks). No code from it is used.

## Other Third-Party Components

Additional third-party components and supporting materials may be present in the BMNeXT source tree. Where such material is accompanied by its own copyright, license, or attribution notices, those notices and terms continue to apply.
