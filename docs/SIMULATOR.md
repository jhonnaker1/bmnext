# Testing and debugging BMNeXT on a PC

BMNeXT isn't tested in QEMU. On the Pi it runs bare metal, on the Circle
kernel with no Linux underneath, and QEMU's Raspberry Pi machines don't
emulate enough of the Pi (its VideoCore display, HDMI sound, USB and Wi-Fi
as BMNeXT uses them) to run it. There would be nothing to strace either:
strace follows a Linux program's system calls, and BMNeXT makes none.

What BMNeXT has instead is a **PC simulator** (`tools/hostsim`, from
BMCoco): the same emulator code that runs on the Pi (Previous and BMNeXT's
front end for it, `src/port/`), built as an ordinary program for Linux or
macOS, with a simulated display, sound, keyboard, mouse, joysticks and SD
card in place of the Pi's. Because it's an ordinary program, **gdb, lldb,
strace and the rest work on it** as on any other program.

## What it can and can't show

The simulator runs the NeXT's emulation, its ROM and NeXTSTEP, the menu,
the disk handling, and BMNeXT's pacing and sound logic. A problem in any
of those usually shows up in it too.

It does **not** run the Pi side: the Circle kernel, the Pi's display and
sound drivers, USB, Wi-Fi and Circle's network stack, and the SD card
driver. And a PC is much faster than a Pi: the simulator says nothing about
whether a Pi keeps up (the menu's speed line, on the Pi, does).

So the simulator is the first place to look, and if a problem doesn't show
up there, the Pi's log (at the end of this page) is the next.

## Setting it up

You need a C compiler, `make`, `git`, `curl`, `unzip` and Python 3. No
Raspberry Pi toolchain is needed for the simulator.

**Linux** (Debian, Ubuntu, Raspberry Pi OS):

```
sudo apt install build-essential git curl unzip python3 gdb strace
```

**macOS**: install Xcode's command line tools (`xcode-select --install`),
which include `clang`, `make`, `git` and the `lldb` debugger.

Get the source, with zlib (a git submodule; Previous is in the
repository itself, in `third_party/previous`):

```
git clone https://github.com/jhonnaker1/bmnext
cd bmnext
git submodule update --init third_party/zlib
```

(`git clone --recursive` works too, but also fetches circle-stdlib, which
only the Pi build needs.)

Build and run the tests, with a folder of your NeXT ROMs (they are not part
of BMNeXT; subfolders are searched too):

```
BMNEXT_ROMS=/path/to/your/roms tools/hostsim/run_tests.sh
```

The first build takes a few minutes: Previous's CPU core is generated C.
The tests start the NeXT Computer, the NeXTstation and the NeXTstation
Turbo Color to their ROM monitors (there is no disk to boot from), type at
the monitor, and check the menu, a CPU that halts, a card with no ROM, the
power key and the web UI's requests.

Each test runs in its own folder under `tools/hostsim/out/`, which stands
in for the SD card, with its log (`log.txt`) and screenshots (`.ppm`).

## Using it live

```
tools/hostsim/live.sh /path/to/your/roms
```

then open <http://127.0.0.1:8064/> in a browser. The NeXT's screen, sound,
keyboard and mouse are in the page, and F12 opens BMNeXT's menu as on the
Pi. The simulator's SD card is `tools/hostsim/out/live`, set up as the
Pi's: your ROMs in `roms`, and a `disks` folder for disk images (a
NeXTSTEP hard disk image there can be chosen in the menu, F12, Disks, SCSI
Disk 0).

Options from `cmdline.txt` go in `BMNEXT_CMDLINE`:

```
BMNEXT_CMDLINE="volume=50" tools/hostsim/live.sh
```

Its messages (the same as the Pi's log, Previous's included) go to the
terminal. Ctrl+C stops it.

## Debugging it

### Where is it stuck? (gdb or lldb)

Start it live as above, get it into the state where it's stuck, and attach
a debugger to it from another terminal.

Linux:

```
sudo gdb -p $(pgrep -f build/bmnext_sim)
(gdb) thread apply all bt
```

macOS:

```
lldb -p $(pgrep -f build/bmnext_sim)
(lldb) thread backtrace all
```

The backtrace shows where each thread is, in BMNeXT's and Previous's
source (the simulator is built with debugging information). When the
emulation is running normally, the main thread is somewhere under
`m68k_go()` (the NeXT's CPU) or `Timing_Sync()` (waiting for real time to
catch up). Type `continue` to let it go on, or `detach` and `quit` to leave it
running.

To run it under the debugger from the start instead, run it from its SD
card folder:

```
cd tools/hostsim/out/live
BMNEXT_LIVE=8064 BMNEXT_CMDLINE="root=." gdb ../../build/bmnext_sim
(gdb) run < /dev/null
```

and press Ctrl+C in gdb when it's stuck.

### Previous's own logs

Previous's settings file on the simulator's SD card (or the Pi's),
`previous.cfg`, has a `[Log]` section: `nTextLogLevel` sets how much
Previous logs, to the same place as BMNeXT's messages.

### Scripts

The simulator can also run a script of key presses, mouse moves and
screenshots on its standard input, with time measured in frames, so that a
problem can be repeated exactly. For example:

```
cd tools/hostsim/out/live
printf '1200 shot monitor.ppm\n1210 key 0x45 1\n1212 key 0x45 0\n1240 shot menu.ppm\n1241 quit\n' |
    BMNEXT_CMDLINE="root=." ../../build/bmnext_sim
```

saves the ROM monitor at frame 1200, presses F12 (USB HID usage 0x45) and
saves the menu at frame 1240. The commands are listed at the top of
`tools/hostsim/host_sim.c`, and `tools/hostsim/run_tests.sh` has many
examples.

Useful settings, as environment variables:

| Variable                        | Does                                                       |
|---------------------------------|------------------------------------------------------------|
| `BMNEXT_CMDLINE`             | Options as in `cmdline.txt` (`root=.` is the SD card)      |
| `BMNEXT_REFRESH`             | The display's refresh rate (default 60)                    |
| `BMNEXT_SIM_RATE`            | The sound output's rate (48000 for a Pi 4 or 5's HDMI)     |
| `BMNEXT_IP`                  | Pretend the network is up with this address (from frame 100) |
| `BMNEXT_WIFI`                | Pretend Wi-Fi is running, with a few networks to choose from |
| `BMNEXT_REFRESH_LATER`       | Change the display's refresh rate to this at frame 30      |

## On the Pi itself

On the Pi there is no debugger or strace, but there is a log. Add
`enable_logging=file` to the line in `cmdline.txt`, and BMNeXT writes
`bmnext.log` in the root of the SD card (and to the serial port: GPIO14
TX and GPIO15 RX at 115200 baud, for a USB serial adapter, which keeps
working even when the SD card doesn't).

What the log shows:

- The Pi, the display and sound, the ROMs found and the machine.
- Previous's own messages: the ROM loaded, disks inserted, the CPU, MMU
  and FPU.
- If the emulation stops moving for 3 seconds, `Emulator has not moved for
  3 s; last seen:` and what it was doing. That line is the most useful one
  for a hang.

When something goes wrong, the most helpful report has:

1. What you did, step by step, and what happened.
2. The BMNeXT version and which Pi.
3. `bmnext.log` from the card, with `enable_logging=file` (taking the
   card out right after the problem; the log is written as it goes).
4. Your `cmdline.txt`, `bmnext.cfg` and `previous.cfg`.
5. If it involves a disk or program, which one, or the file if it's free
   to share.
