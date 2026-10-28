# Changelog

## 0.1.0 (2026-10-28)

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
- Blank hard disks (105 MB to 2.8 GB, the NeXT's own drive sizes) and
  floppies made from the menu
- Boot options in the menu (Machine, Boot): what the NeXT starts from and
  its power-on tests; a hard disk inserted in SCSI disk 0 is started from
- The mouse scaled with acceleration, as in Previous, with a Mouse Speed
  setting
- No NeXTdimension yet (planned); the NeXTstation Color and Turbo Color
  give colour
- Keys reach the NeXT one at a time: its keyboard holds one event, so keys
  pressed and let go close together (fast typing) were lost, or stayed down
- The web UI and the live simulator's page in a NeXT style, NeXTSTEP's
  greys, with "NeXT" spelt as it is
- The NeXT's network (Machine, Ethernet Connected): Previous's SLiRP and
  its NetInfo, NFS and other servers, on one thread, over BMNeXT's own
  sockets (127.0.0.1 in memory, the rest through the Pi's network);
  NeXTSTEP gets 10.0.2.15 and starts with it. Tested in the PC simulator
- The PC simulator's tests boot each kind of NeXT to its ROM monitor, with
  the user's ROMs (BMNEXT_ROMS)

- First tests on a Raspberry Pi 5: OPENSTEP 4.2 starts to its login
  window on a NeXTstation Turbo Color, at the real machine's speed or
  more (Variable Speed), with the web UI and the network working. Fixed
  on the way:
  - A black screen at start: Previous asks for its memory 64K aligned,
    which Circle's memalign() can't do (it asserts, and the panic stops
    every core before anything is shown); plain malloc() on the Pi now. A
    panic on any core now writes the log to the card before it stops
  - "Error saving config file", and settings lost: Previous saves
    previous.cfg through tmpfile(), which on the Pi wanted a /tmp the card
    hasn't (and deletes the file while it is open, which FatFs doesn't
    stand); its temporary files are in memory now
  - Very slow (5 to 10 minutes to start OPENSTEP): the whole 1120x832
    picture was converted and written out sixty times a second, most of
    the emulator's time; now only the lines that changed, and nothing when
    the picture is the same (the Pi 4 and 5's display driver)
  - The Pi 5 is asked for its full clock (2.4 GHz), as the Pi 4 is
  - FatFs's fast seek for big files (disk images): seeking back in a file
    no longer reads its cluster chain from the start (patches to Circle
    and circle-newlib, see src/patches/README.md)
  - The log notes every 10 s how fast the NeXT runs, the CPU's clock and
    temperature, and where the emulator's time went
  - Sound too fast and high, then (once paced) too slow and low: Previous
    sends the NeXT's sound faster than it plays and waits while the output
    says it has enough, which BMNeXT never said; and the Pi 5's HDMI takes
    32 ms at a time, too much for the 60 ms buffer, which the conversion
    took for running dry and stretched. Now the output says, the buffer is
    120 ms, and 44.1 kHz becomes 48 kHz at exactly their ratio
  - HDMI sound's clock regeneration (CTS) set for the mode the firmware
    chose, which the Pi 5's sound worked out from the wrong clock
  - Sound, Test Tone and Test Tone (NeXT's Way) in the menu: 440 Hz
    straight to the output, or made at 44.1 kHz the NeXT's way
  - The A key on the Turbos' Apple (ADB) keyboard: Apple's key code for A
    is 0, which the key table took for no key
  - Keys to the Apple keyboard at most every 15 ms: a burst of them
    overran its 16 events, losing keys, or a key's release (the key then
    repeated)
  - No default route for the NeXT, so no connections out: SLiRP's routing
    broadcasts give the address family as the C library's AF_INET, which
    is 1 in Circle's, not BSD's 2, so the NeXT's routed ignored them; they
    get 2 on their way to the NeXT now
- The NeXT needs `/etc/resolv.conf` (`nameserver 10.0.2.3`) for names;
  ping to the Internet doesn't work (SLiRP's, as in Previous)
- Disk images are at most 2 GB on the Pi (circle-newlib's lseek())

Tested in the PC simulator, where NeXTSTEP 3.3 starts to its login window,
and on a Raspberry Pi 5; not yet on a Pi 4.

## Earlier: BMAtariST

BMNeXT's history before this is BMAtariST's (and before it BMApple2's,
BMCoco's and BMA800XL's); see the git log.
