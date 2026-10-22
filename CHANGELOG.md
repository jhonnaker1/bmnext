# Changelog

## Unreleased

The first BMNeXT: BMAtariST 0.1.0 (itself from BMApple2, BMCoco, BMA800XL
and BMC64), with Hatari replaced by Previous (SVN trunk r1858, Previous
4.5), for the Raspberry Pi 4 and 5.

- Emulates the NeXT Computer (68030), NeXTcube, NeXTcube Turbo,
  NeXTstation, NeXTstation Turbo, NeXTstation Color and NeXTstation Turbo
  Color, with Previous's CPU, MMU, FPU, DSP and chips
- Previous on one thread (its single thread mode); no NeXTdimension, no
  network yet
- The user's own ROMs in the roms folder, found by size
- SCSI hard disk, CD-ROM and floppy images, optical disks and the floppy
  drive, from the menu
- The MegaPixel Display's 1120x832 in four greys or 4096 colours
- USB keyboards by key position (Windows is Command, F10 Power), USB mice
- Sound at 44.1 kHz, resampled to the output's rate
- A CPU that halts (the wrong ROM for the machine) stops and says so, and
  a NeXT that switches itself off waits for a key
- The PC simulator's tests boot each kind of NeXT to its ROM monitor, with
  the user's ROMs (BMNEXT_ROMS)

Tested in the PC simulator only, where NeXTSTEP 3.3 starts to its login
window; not yet on a Raspberry Pi.

## Earlier: BMAtariST

BMNeXT's history before this is BMAtariST's (and before it BMApple2's,
BMCoco's and BMA800XL's); see the git log.
