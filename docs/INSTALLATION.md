# Installing BMNeXT

## What you need

- A Raspberry Pi 5, Pi 500 or Pi 500+, or a Raspberry Pi 4 Model B or Pi
  400. The NeXT's 68030 and 68040 (with their MMUs) need one of these.
- A microSD card (BMNeXT itself needs only a few MB; a NeXTSTEP disk image
  is usually 400 MB to 2 GB).
- An HDMI display that accepts 1080p at 60 Hz (at 720p the NeXT's
  1120x832 picture is shrunk to fit).
- A USB keyboard and a USB mouse.
- Your NeXT's ROMs (see [ROMS.txt](ROMS.txt)): they are copyrighted, so
  BMNeXT can't include them. And, to do more than the ROM monitor, a disk
  image with NeXTSTEP or OPENSTEP installed.

## Preparing the SD card

1. Format the card as FAT32.
2. Copy everything in the release zip (or in `build/sdcard/` after
   building) to the root of the card: the kernels (`kernel_2712.img`,
   `kernel8-rpi4.img`), the firmware, `config.txt`, `cmdline.txt` and the
   folders.
3. Copy your ROMs into the `roms` folder.
4. Copy your disk images to the card. The `disks` folder is there for
   them, but any folder works.

## First start

The NeXT tests itself (its memory, the FPU, SCSI and the rest), which takes
a few seconds, and then starts from its disk, or stops at the ROM monitor
(`NeXT>`) when there is none. Press **F12** for the menu: *Disks*, *SCSI
Disk 0*, *Insert a Disk Image...* puts a disk image in, and the NeXT starts
again from it. *Machine* chooses which NeXT it is.

The display is set to 1080p at 60 Hz: the NeXT's 832 lines need 1080 to be shown pixel for pixel. If you get no picture, see
[Troubleshooting](USER_GUIDE.md#troubleshooting) in the User Guide.

## Networking (optional)

To manage the card's files from a browser, and start disk images from it,
turn on Ethernet or Wi-Fi under *Network* in the F12 menu, then reboot; the
web UI's address is shown at the bottom of the screen. For Wi-Fi, *Scan
for Wi-Fi Networks* lists the networks in range and asks for the password.
See [Networking and the web UI](USER_GUIDE.md#networking-and-the-web-ui).

## Upgrading

Copy the new release's files over the old ones, except for files you have
changed: `config.txt` and `cmdline.txt` if you edited them. Your settings
(`bmnext.cfg`, `previous.cfg`, `wpa_supplicant.conf`), your ROMs and disk
images are not in the release zip and stay as they are.

With networking on, a new kernel (`kernel_2712.img` for a Pi 5, 500 or
500+, `kernel8-rpi4.img` for a Pi 4 or 400) can also be uploaded in the web UI's file manager and started with
**Reboot**. Updates that change other files (the Pi firmware in the root or
in `firmware/`) need the card in a computer.
