BMNeXT - bare metal NeXT emulator for the Raspberry Pi 4 and 5
==============================================================

Copy everything in this folder to the root of a FAT32 SD card and start a
Raspberry Pi 5, 500, 4 or 400 from it, with a USB keyboard and mouse.

The NeXT needs its ROM, which is copyrighted and not included: copy yours
into the roms folder (see roms/ROMS.txt). With no disk to start from, the
NeXT stops at its ROM monitor, the "NeXT>" prompt.

NeXTSTEP: copy a hard disk image with NeXTSTEP (or OPENSTEP) installed to
the disks folder, press F12, Disks, SCSI Disk 0, Insert a Disk Image, and
choose it; Restart (in the menu) starts the NeXT from it.

Keys:
  F12               BMNeXT's menu       Windows           Command
  Alt               Alternate           F10 or Delete     Power
  F5/F6, End/Home   Volume down/up      F1/F2, PgDn/PgUp  Brightness

Networking: choose Ethernet or Wi-Fi under Network in the menu (or add
network=ethernet or network=wifi to cmdline.txt), then open the address
shown on screen in a browser to manage the files on this card and start
disk images. (The NeXT itself has no network yet.)

No need to shut down: just switch it off (shutting NeXTSTEP down first
keeps its disk tidy).

Licences: BMNeXT is under the GPL version 3 (LICENSE.txt); the licenses
folder has the notices for other projects' code it includes (Previous,
zlib and the rest), and the firmware folder the Raspberry Pi and Wi-Fi
firmware's licences.

Options are in cmdline.txt; see docs/USER_GUIDE.md in BMNeXT's source
for all of them: https://github.com/jhonnaker1/bmnext
