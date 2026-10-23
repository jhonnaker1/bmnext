# BMNeXT User Guide

BMNeXT turns a Raspberry Pi 4 or 5 into a NeXT: the NeXT Computer (the
68030 Cube), the NeXTcube, NeXTstation and their Turbo and Color models.
It runs the Previous emulator on the bare hardware, so it starts in a couple
of seconds and can be switched off at any time.

BMNeXT is new: it has been tested in the PC simulator, not yet on a
Raspberry Pi. How fast NeXTSTEP runs on a Pi is not known yet.

## The SD card

| File or folder   | What it is                                                  |
|------------------|-------------------------------------------------------------|
| `kernel8-rpi4.img`, `kernel_2712.img` | BMNeXT for the Pi 4 and Pi 400, and the Pi 5, 500 and 500+ |
| `bootcode.bin`, `start.elf`, `fixup.dat`, `start4.elf`, `fixup4.dat`, `armstub8-rpi4.bin`, `*.dtb`, `overlays/` | Raspberry Pi firmware |
| `config.txt`     | Raspberry Pi firmware settings: the display mode            |
| `cmdline.txt`    | BMNeXT's options (see Options)                              |
| `bmnext.cfg`     | Settings changed in BMNeXT's menu (written by BMNeXT)       |
| `previous.cfg`   | Previous's settings: the machine, ROMs, memory, the disks (written by Previous when the menu changes them) |
| `roms/`          | The NeXT's ROMs: your own (see ROMs)                        |
| `disks/`         | For your disk images (any folder works)                     |
| `firmware/`      | Wi-Fi firmware                                              |

The NeXT writes straight to its disk images on the card, and BMNeXT pushes
everything out to the card every second, so the Pi can be switched off at
any time; but as with a real NeXT, shutting NeXTSTEP down first (its Power
Off) keeps its disk tidy.

## ROMs

A NeXT starts from its ROM. NeXT's ROMs are copyrighted and not included:
copy images of yours into the card's `roms` folder (see
[ROMS.txt](ROMS.txt)). BMNeXT finds them by size: 64K for the NeXT Computer
(ROM 0.8 to 1.2), 128K for the 68040 machines (ROM 2.x) and the Turbos
(ROM 3.x, told by a name with "3." in it, as `Rev_3.3_v74.BIN`). Another can
be chosen in the menu: *Machine*, *ROM*.

A new card's NeXT starts in its ROM monitor, the `NeXT>` prompt (`help`
there lists its commands), until a hard disk is put in SCSI disk 0 (see
Disks); then it starts from that disk.

## Disks

*Disks* in the menu has the NeXT's drives:

- **SCSI Disk 0 to 6**: each a hard disk, a CD-ROM or a floppy drive, with
  a disk image. NeXTSTEP and OPENSTEP are installed on a hard disk image
  (a raw image of the whole disk). Inserting a hard disk in SCSI disk 0
  also makes the NeXT start from it (*Machine*, *Boot*, *Start From*
  chooses otherwise).
- **Optical Drive 0, 1**: the NeXT Computer's magneto-optical drives, with
  optical disk images.
- **Floppy Drive**: the 2.88 MB floppy drive of the 68040 machines.

Changing a hard disk restarts the NeXT.

**Blank disks**: *Create a Blank Hard Disk...* (in a SCSI disk's menu) and
*Create a Blank Disk...* (the floppy drive's) make an empty disk image in
the card's `disks` folder and insert it, for installing NeXTSTEP on (from
a CD-ROM image in another SCSI disk) or for more space. The sizes are those
of the NeXT's own drives, which Previous recognises: 105 MB to 2.8 GB hard
disks (the two "early NeXTSTEP" ones for NeXTSTEP before 2.0), and 720 KB,
1.44 MB and 2.88 MB floppies. NeXTSTEP's BuildDisk (or Disk.app) then
labels and formats it. A big one takes a while to write to the SD card;
the menu shows how far it is.

## The menu

**F12** (or a gamepad's Home, or Select and Start together) opens the menu,
and the NeXT stops while it's open. The arrow keys (or the D-pad) move,
**Enter** (or A) chooses, **Esc** (or B) goes back, and F12 again closes it.

| Item          | What it has |
|---------------|-------------|
| Disks         | SCSI disks, optical drives and the floppy drive; blank disks |
| Restart       | Switching the NeXT off and on |
| Machine       | The machine, its ROM, memory, what it starts from (Boot), variable speed, swapping Command and Alt, mouse speed |
| Video         | Screen size, CRT effect, smooth scaling, disk activity |
| Sound         | Volume, HDMI or the headphone jack |
| Network       | The Pi's Ethernet or Wi-Fi, the web UI |
| About         | Versions, the machine, the display, how fast the emulation runs |

The menu's last line shows how fast the emulation runs, as a percentage of
the real machine's speed. Under 100% the Pi can't keep up with the machine.

The NeXT's settings (machine, ROM, disks) are Previous's, kept in
`previous.cfg`; a change that needs it restarts the NeXT, as in Previous's
own settings dialog. BMNeXT's own (video, sound, network) are kept in
`bmnext.cfg`.

## Keyboard and mouse

Keys go to the NeXT by where they are on the keyboard. The NeXT's keys a PC
keyboard doesn't have, as in Previous:

| PC key                   | NeXT key                  |
|--------------------------|---------------------------|
| Windows (Command on a Mac keyboard) | Command        |
| Alt                      | Alternate                 |
| Control                  | Control                   |
| F10 or Delete            | Power                     |
| F5 and F6, End and Home  | Sound volume down and up  |
| F1 and F2, Page Down and Page Up | Brightness down and up |

*Machine*, *Swap Command and Alt* swaps those two. Caps Lock holds Shift,
as on a NeXT keyboard (which has none). F12 is BMNeXT's menu; the Pi's own
power key (a Pi 400's Fn+F10) shuts the Pi down.

A USB mouse is the NeXT's mouse, with its left and right buttons. Its
movements are scaled, with some acceleration, as in Previous;
*Machine*, *Mouse Speed* makes it slower or faster (`fLinScale` and
`fExpScale` in `previous.cfg`'s `[Mouse]` section are the exact
settings).

## Boot

*Machine*, *Boot* has Previous's boot options, which the NeXT keeps in its
NVRAM: what it starts from (the ROM monitor, a SCSI disk, the network, an
optical disk or the floppy), and its power-on tests (the memory test, the
power-on test with its sound and SCSI parts, verbose output, extended
diagnostics). A change restarts the NeXT. The ROM monitor has more: `p`
sets the boot settings, `b` starts from a device, as `b sd(1,0,0)` for SCSI
disk 1.

## Machines and speed

*Machine*, *Machine* chooses the NeXT, each with its own CPU, as Previous
sets them: the NeXT Computer's 68030 at 25 MHz, the 68040 at 25 MHz in the
NeXTcube and NeXTstation, at 33 MHz in the Turbos. The Color models show
4096 colours, the others four greys. *Memory* is 8 to 128 MB.

Previous normally keeps the NeXT's time by its CPU: a busy NeXT on a Pi
that can't keep up runs slow, clock and all. *Variable Speed* runs the
NeXT's clock in real time instead while programs run, as Previous's own
option does, so it keeps time whatever the Pi manages.

Not emulated: the NeXT's Ethernet (it sees a network with no cable), sound
input, the laser printer, and the NeXTdimension board (the NeXTcube's
32-bit colour graphics board with its own Intel i860, planned for later).
For colour, choose the NeXTstation Color or NeXTstation Turbo Color: 4096
colours on the same display, which NeXTSTEP shows in colour as it is.

## Display

The NeXT's MegaPixel Display is 1120x832, scaled to fill the screen at its
own shape (square pixels): on a 1080p display (`config.txt`'s default)
pixel for pixel, with black bars around it; on a 720p one shrunk to fit.
The display runs at 60 Hz; the NeXT's own 68 Hz frames are shown as they
come.

## Networking and the web UI

BMNeXT can join your network over Ethernet or Wi-Fi and serve a web page
for managing the SD card from any browser on the same network, so files can
be copied to and from the Pi without taking the card out. It is the web UI
from BMC64.

Turn it on in the menu's *Network*. For Ethernet, set *Network* to Ethernet
and reboot. For Wi-Fi, choose *Scan for Wi-Fi Networks*: the first time,
it offers to switch Wi-Fi on and reboot; after that it lists the networks
it can hear, strongest first. Pick yours: it fills in *Wi-Fi Network*, and
if the network needs a password, *Wi-Fi Password* is next. Then reboot.
Set *Wi-Fi Country* to your two letter country code too; it decides which
channels are allowed. A hidden network can be typed in under *Wi-Fi
Network* instead. Or, in `cmdline.txt` (on the same single line as the
other options):

- Ethernet: `network=ethernet`
- Wi-Fi: `network=wifi`, and copy `wpa_supplicant.conf.example` to
  `wpa_supplicant.conf` in the root of the card with your network's name,
  password and country.

Wi-Fi needs the files in the `firmware` folder, which `build_sdcard.sh` puts
there.

Once the Pi has an address, it is shown at the bottom of the screen for a
few seconds (`WEB UI: HTTP://192.168.1.50/`) and written to the log.
Open that address in a browser.

The web UI has:

- **Dashboard**: the Pi's model, address, uptime, temperature, power and
  throttling warnings and free space on the card. Buttons to **Restart**
  the NeXT (as switching it off and on), **Reboot** BMNeXT, **Shut
  Down** (the Pi halts; switch it off, then on again to restart; a Pi 400
  switches itself off, and its power key starts it again), and **Disable
  Web UI** until the next reboot.
- **Files**: browse the SD card; upload (or drag and drop) files into a
  folder, download, rename, delete, and create folders. Clicking a disk
  image (`.dsk`, `.img`, `.dd`, `.hd`, `.raw`) **starts it**: it becomes
  the NeXT's first SCSI hard disk and the NeXT starts from it. `cmdline.txt`, `config.txt`,
  `bmnext.cfg`, `previous.cfg` and `wpa_supplicant.conf` can be **edited**
  in the browser; changes take effect after a reboot, and the previous
  version is kept as `.bak`.

Uploads can't overwrite `cmdline.txt`, `config.txt` or `wpa_supplicant.conf`
(edit them instead), or anything in `firmware`. Everything else, including
the kernels, can be replaced, so a new build of BMNeXT can be uploaded and
started with **Reboot**. A mistake in `cmdline.txt` or `config.txt` can stop
the Pi starting, and then needs the card in a computer (the `.bak` copy
makes that easy).

The web UI is open to anyone on your network. To protect it with a PIN, set
`webui_pin=1234` in `cmdline.txt`; the browser then asks for a user name
(anything) and the PIN. To network without the web UI, set `webui=0`.

The network also sets the Pi's clock, so files saved on the card get the
right date and time. Set `timezone` to your offset from UTC in minutes, for
example `timezone=600` for UTC+10. (Previous sets the NeXT's clock to the Pi's date and time when it starts.)

## Options

`cmdline.txt` is a single line of `name=value` options separated by spaces.
BMNeXT's menu saves the options it changes to `bmnext.cfg`, one
`name=value` per line, and those win over `cmdline.txt`. Either file can be
edited from the web UI. Everything about the emulated machine is in
Previous's settings, `previous.cfg`, which the menu changes.

| Option              | Values                       | Default  | What it does |
|---------------------|------------------------------|----------|--------------|
| `aspect`            | number                       | `1.0`    | Multiplies the width of the picture |
| `disk_activity`     | `0`, `1`                     | `1`      | Show which disk the NeXT is using at the bottom left (Video, Show Disk Activity) |
| `scale`             | 0.1 to 1.0                   | `1.0`    | Height of the picture as a fraction of the screen (Video, Screen Size; less for a TV that cuts off the edges) |
| `smooth`            | `0`, `1`                     | `0`      | Bilinear filtering instead of sharp pixels |
| `crt_shader`        | `0`, `1`                     | `0`      | CRT look (scanlines) from BMC64 |
| `crt_curvature`     | `0`, `1`                     | `0`      | With `crt_shader`, a curved screen |
| `crt_gpu`           | `0`, `1`                     | `1`      | Pi 4 and 5: the 3D GPU draws the smooth and CRT pictures; `0` has the ARM draw them (see [PI4.md](PI4.md)) |
| `resolution`        | `auto`, `1920x1080`, `1280x720` | `auto` | Pi 5 only, with `video_driver=kms`: the display's resolution; `auto` keeps the one the firmware chose (the Pi 5's firmware ignores `hdmi_mode`) |
| `video_driver`      | `kms`, `hvs`, `firmware`     | `kms`    | Pi 4 and 5: `kms` sets the display mode itself (60 Hz); `hvs` leaves the mode set in `config.txt`; `firmware` uses the firmware's framebuffer, if the others give trouble (see [PI4.md](PI4.md) and [PI5.md](PI5.md)) |
| `sound`             | `0`, `1`                     | `1`      | Sound on or off |
| `volume`            | 0 to 100                     | `100`    | Sound volume |
| `audio_out`         | `auto`, `hdmi`, `headphones` | `auto`   | Where the sound goes (`auto`: HDMI, or the headphone jack if the display takes no sound) |
| `audio_buffer_ms`   | 20 to 500                    | `60`     | Sound buffer; raise it if the sound breaks up |
| `enable_logging`    | `off`, `uart`, `file`        | `off`    | Where the log goes (see Logging) |
| `network`           | `off`, `ethernet`, `wifi`    | `off`    | Networking (see Networking and the web UI) |
| `webui`             | `0`, `1`                     | `1`      | Web UI when networking is on |
| `webui_pin`         | digits                       | none     | PIN for the web UI |
| `timezone`          | minutes east of UTC          | `0`      | Time zone for the network clock |
| `usbpowerdelay`     | milliseconds                 | Circle's | Circle: wait for USB devices to power up (by default 300 ms, 510 ms behind a hub) |

## Logging

BMNeXT logs what it is doing, which helps when something doesn't work:
the version, the display and sound set up, the ROMs found, devices coming
and going, network status, and Previous's own messages (the ROM loaded,
disks inserted, anything it didn't like). Choose
where it goes with `enable_logging` in `cmdline.txt`:

- **Off** (the default, also when `enable_logging` isn't set): no log.
- **UART**: the serial port, GPIO14 TX and GPIO15 RX at 115200 baud
  (all boards, the Pi 5 too). Needs a USB serial adapter.
- **File**: the serial port, and `bmnext.log` in the root of the SD
  card. The file starts with what was logged since boot, is written to the
  card every few seconds (and is complete after a crash, a hang or
  switching off), and starts again at boot once it is over 1 MB. Download
  it from the web UI's file manager to send with a bug report. Writing it
  costs a little time on each frame, so switch it off once you have what
  you need.

## Troubleshooting

**"No ROM for the NeXT Computer" (or another machine)**: there is no ROM
for the machine in the card's `roms` folder (see ROMs). Copy yours there.

**"The NeXT's CPU halted"**: usually a ROM that isn't the machine's (a 2.x
or 3.x ROM on the 68030 NeXT Computer, or the other way round). Choose the
machine again in the menu (Machine, Machine), which sets its CPU, and
check Machine, ROM.

**The NeXT stops at the "NeXT>" prompt**: that is its ROM monitor, where a
NeXT with no disk to start from ends up. Insert a disk image with NeXTSTEP
(Disks, SCSI Disk 0), which restarts it from that disk; or type `b` and
Enter at the prompt to start from the boot device (Machine, Boot).

**"boot sd(0,0,0)" and nothing more**: the NeXT is waiting for a SCSI disk
that isn't there (a Turbo's ROM waits a long time for one to spin up).
Insert one, or set Machine, Boot, Start From to the ROM Monitor.

**The mouse pointer is too fast or too slow**: Machine, Mouse Speed.

**No picture**: uncomment `hdmi_safe=1` in `config.txt`. If that works, your
display doesn't accept 1080p at 60 Hz; try another `hdmi_mode`.

**No sound over HDMI**: try `hdmi_drive=2` in `config.txt`, or
`audio_out=hdmi` in `cmdline.txt`.

**Sound breaks up, or the menu's speed line is under 100%**: the Pi can't
keep up with that machine. The NeXT Computer (68030, 25 MHz) and the
68040 machines at 25 MHz take less than the Turbos (33 MHz). For the sound
alone, raise `audio_buffer_ms` in `cmdline.txt`, e.g. to 100.

**The TV cuts off the edges of the picture or the menu**: many TVs enlarge
the picture and crop its edges (overscan). Make *Video*, *Screen Size*
smaller (90% usually does), or turn overscan off on the TV ("Just Scan",
"Screen Fit" or "1:1").

**Keyboard or mouse not working**: unplug it and plug it in again; devices
can come and go while the emulator runs. Some keyboards need a longer
`usbpowerdelay` (add `usbpowerdelay=1000` to `cmdline.txt`).
