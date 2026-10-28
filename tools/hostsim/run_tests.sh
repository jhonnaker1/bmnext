#!/bin/bash
#
# Runs the emulator port on the PC (see host_sim.c) through a set of
# scripted tests and checks the results.
#
#   BMNEXT_ROMS=folder tools/hostsim/run_tests.sh
#   tools/hostsim/run_tests.sh --bmnet-only    (the network's sockets only)
#
# The NeXT's ROMs are not part of BMNeXT: BMNEXT_ROMS is a folder with
# yours (searched with its subfolders for 64K and 128K .bin files, as
# Previous's Rev_1.0_v41.BIN, Rev_2.5_v66.BIN and Rev_3.3_v74.BIN). The
# tests boot each NeXT to its ROM monitor (there is no disk to start
# from). Output, including screenshots (.ppm), goes to tools/hostsim/out/.
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
SIM=$HERE/build/bmnext_sim

# The network's sockets (bmnet.c) first: they need no ROMs.
make -s -C "$HERE" build/bmnet_test || exit 1
if ! "$HERE/build/bmnet_test"; then
    echo "bmnet's tests failed." >&2
    exit 1
fi
if [ "${1:-}" = "--bmnet-only" ]; then
    exit 0
fi

if [ -z "${BMNEXT_ROMS:-}" ] || [ ! -d "$BMNEXT_ROMS" ]; then
    echo "Set BMNEXT_ROMS to a folder with your NeXT ROMs." >&2
    exit 2
fi

make -s -C "$HERE" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" || exit 1

rm -rf "$OUT"
mkdir -p "$OUT/roms"
find "$BMNEXT_ROMS" -iname '*.bin' \( -size 65536c -o -size 131072c \) -exec cp {} "$OUT/roms/" \;

FAILED=0
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1: $2"; FAILED=$((FAILED + 1)); }

# run NAME CMDLINE SCRIPT [SYSTEM]: runs the simulator in out/NAME, a
# stand-in SD card with the ROMs in its roms folder; SYSTEM, if given, is
# previous.cfg's [System] section. At most two minutes.
run() {
    local name=$1 cmdline=$2 script=$3 system=${4:-}
    mkdir -p "$OUT/$name"
    ln -s "$OUT/roms" "$OUT/$name/roms" 2>/dev/null
    [ -n "$system" ] && printf "[System]\n$system" > "$OUT/$name/previous.cfg"
    (cd "$OUT/$name" && printf "$script" | BMNEXT_CMDLINE="root=. $cmdline" "$SIM" > log.txt 2>&1 &
     pid=$!; (sleep 120; kill $pid 2>/dev/null) & killer=$!; wait $pid; kill $killer 2>/dev/null)
}

# check_log NAME REGEX MESSAGE: the log must contain REGEX
check_log() {
    if grep -Eq "$2" "$OUT/$1/log.txt"; then
        pass "$1: $3"
    else
        fail "$1" "$3 (see out/$1/log.txt)"
    fi
}

# check_cfg NAME FILE REGEX MESSAGE: out/NAME/FILE must have a line REGEX
check_cfg() {
    if grep -Eq "$3" "$OUT/$1/$2" 2>/dev/null; then
        pass "$1: $4"
    else
        fail "$1" "$4 (not in out/$1/$2)"
    fi
}

# count NAME FILE R G B: how many pixels of that colour the screenshot has
count() {
    python3 - "$OUT/$1/$2.ppm" "$3" "$4" "$5" <<'EOF'
import sys
try:
    data = open(sys.argv[1], 'rb').read()
except OSError:
    print(0)
    sys.exit(0)
parts = data.split(b'\n', 3)
px = parts[3]
want = bytes(int(v) for v in sys.argv[2:5])
print(sum(1 for i in range(0, len(px), 3) if px[i:i + 3] == want))
EOF
}

# monitor NAME FILE: a ROM monitor's window (white, 0x000 in the NeXT's
# greys) fills much of the screen
monitor() {
    local n
    n=$(count "$1" "$2" 255 255 255)
    if [ "$n" -gt 300000 ]; then
        pass "$1: the ROM monitor's window ($2)"
    else
        fail "$1" "no ROM monitor in $2.ppm ($n white pixels)"
    fi
}

same() {
    if [ -f "$OUT/$1/$2.ppm" ] && [ -f "$OUT/$1/$3.ppm" ] && cmp -s "$OUT/$1/$2.ppm" "$OUT/$1/$3.ppm"; then
        pass "$1: $2 and $3 are the same"
    else
        fail "$1" "$2 and $3 differ"
    fi
}
differ() {
    if [ -f "$OUT/$1/$2.ppm" ] && [ -f "$OUT/$1/$3.ppm" ] && ! cmp -s "$OUT/$1/$2.ppm" "$OUT/$1/$3.ppm"; then
        pass "$1: $2 and $3 differ"
    else
        fail "$1" "$2 and $3 should differ"
    fi
}

# USB HID usages used below.
F12=0x45 DOWN=0x51 ENTER=0x28 ESC=0x29 POWER=0x66 A=0x04
# KEY USAGE FRAME: press at FRAME, release two frames later
key() { printf '%d key %s 1\\n%d key %s 0\\n' "$2" "$1" $(($2 + 2)) "$1"; }

# [System] settings as Previous's Configuration_SetSystemDefaults() makes
# them for each machine.
STATION='nMachineType = 2\nbTurbo = FALSE\nbColor = FALSE\nnCpuLevel = 4\nnCpuFreq = 25\nn_FPUType = 68040\nnSCSI = 1\nnRTC = 0\nbADB = FALSE\nbNBIC = FALSE\nbDSPMemoryExpansion = TRUE\n'
TURBOCOLOR='nMachineType = 2\nbTurbo = TRUE\nbColor = TRUE\nnCpuLevel = 4\nnCpuFreq = 33\nn_FPUType = 68040\nnSCSI = 1\nnRTC = 1\nbADB = TRUE\nbNBIC = FALSE\nbDSPMemoryExpansion = TRUE\n'

echo "The NeXT Computer (68030) starts to its ROM monitor"
run boot "" '900 shot a.ppm\n1800 shot b.ppm\n1801 quit\n'
check_log boot "Loading ROM from .*roms/" "a ROM found in roms/"
check_log boot "CPU=68030, FPU=68882, MMU=68030" "a 68030 with its FPU and MMU"
check_cfg boot previous.cfg "^szRom030FileName = .*roms/" "a new card's previous.cfg names the ROM"
monitor boot b
same boot a b

echo "The NeXTstation (68040, ROM 2.x) and NeXTstation Turbo Color (ROM 3.x)"
run station "" '1800 shot a.ppm\n1801 quit\n' "$STATION"
check_log station "CPU=68040, FPU=68040, MMU=68040" "a 68040"
check_log station "Loading ROM from .*roms/.*2" "the 2.x ROM"
monitor station a
run turbocolor "" '1800 shot a.ppm\n1801 quit\n' "$TURBOCOLOR"
check_log turbocolor "Loading ROM from .*roms/.*3" "the 3.x ROM"
monitor turbocolor a

echo "Typing at the ROM monitor"
run typing "" "1200 shot before.ppm\n1210 type help\n$(key $ENTER 1260)1500 shot after.ppm\n1501 quit\n"
differ typing before after
# The same keys, each pressed and let go in the same frame as the others
# (fast typing, a key let go as the next is pressed): the NeXT's keyboard
# holds one event at a time, so they must reach it one by one.
FAST=""
for u in 0x0b 0x08 0x0f 0x13 $ENTER; do FAST="${FAST}1210 key $u 1\n1210 key $u 0\n"; done
run fasttype "" "${FAST}1500 shot after.ppm\n1501 quit\n"
cp "$OUT/typing/after.ppm" "$OUT/fasttype/typed.ppm"
same fasttype typed after

echo "The menu"
run menu "" "1200 shot before.ppm\n$(key $F12 1210)1230 shot menu.ppm\n$(key $ESC 1240)1260 shot after.ppm\n1261 quit\n"
differ menu before menu
same menu before after
run padmenu "" '1200 pad 0 0\n1210 pad 0 0x0100\n1214 pad 0 0\n1230 shot menu.ppm\n1231 quit\n'
cp "$OUT/menu/menu.ppm" "$OUT/padmenu/f12menu.ppm"
same padmenu f12menu menu

echo "The menu's settings"
# Menu, Machine (two down), Mouse Speed (seven down), the last: Fastest.
run mouse "" "$(key $F12 600)$(key $DOWN 610)$(key $DOWN 615)$(key $ENTER 620)$(key $DOWN 650)$(key $DOWN 655)$(key $DOWN 660)$(key $DOWN 665)$(key $DOWN 670)$(key $DOWN 675)$(key $DOWN 680)$(key $ENTER 690)$(key $DOWN 720)$(key $DOWN 725)$(key $DOWN 730)$(key $DOWN 735)$(key $ENTER 740)$(key $F12 760)800 quit\n"
check_cfg mouse previous.cfg "^fLinScale = 2" "Machine > Mouse Speed saved in previous.cfg"
# Menu, Machine, Boot (three down), Start From, SCSI Disk (one down).
run boot_menu "" "$(key $F12 600)$(key $DOWN 610)$(key $DOWN 615)$(key $ENTER 620)$(key $DOWN 630)$(key $DOWN 635)$(key $DOWN 640)$(key $ENTER 650)$(key $ENTER 680)$(key $DOWN 710)$(key $ENTER 720)$(key $F12 760)800 quit\n"
check_cfg boot_menu previous.cfg "^nBootDevice = 1" "Machine > Boot > Start From saved in previous.cfg"
check_log boot_menu "boot options reset" "the NeXT restarts for it"
# Menu, Machine, Ethernet Connected (four down): the NeXT's network on.
run ethernet "" "$(key $F12 600)$(key $DOWN 610)$(key $DOWN 615)$(key $ENTER 620)$(key $DOWN 630)$(key $DOWN 635)$(key $DOWN 640)$(key $DOWN 645)$(key $ENTER 650)$(key $F12 700)1200 quit\n"
check_cfg ethernet previous.cfg "^bEthernetConnected = TRUE" "Machine > Ethernet Connected saved in previous.cfg"
# Menu, Disks, SCSI Disk 0, Create a Blank Hard Disk (two down), 105 MB.
run blank "" "$(key $F12 600)$(key $ENTER 610)$(key $ENTER 620)$(key $DOWN 650)$(key $DOWN 655)$(key $ENTER 660)$(key $ENTER 690)$(key $F12 910)1200 quit\n"
if [ "$(wc -c < "$OUT/blank/disks/blank105.sd" 2>/dev/null)" -eq 110100480 ] 2>/dev/null &&
   cmp -s -n 110100480 "$OUT/blank/disks/blank105.sd" /dev/zero; then
    pass "blank: a 105 MB blank disk, all zeros, in disks/"
else
    fail "blank" "no 105 MB blank disk in out/blank/disks/"
fi
check_cfg blank previous.cfg "^szImageName0 = .*disks/blank105\.sd" "it is SCSI disk 0"
check_log blank "SCSI disk 0: Insert .*blank105\.sd" "the NeXT has it"

echo "The network (SLiRP)"
# Started from the network: the ROM asks for its address (BOOTP), which
# SLiRP answers, and loads its boot file over TFTP.
run netboot "" '3000 shot a.ppm\n3001 quit\n' 'nMachineType = 0\n[Boot]\nnBootDevice = 2\n[Ethernet]\nbEthernetConnected = TRUE\n'
check_log netboot "Starting SLIRP" "SLiRP starts"
check_log netboot "network: first packet from the NeXT" "the NeXT's packets reach SLiRP"
check_log netboot "network: first packet to the NeXT" "SLiRP's answers reach the NeXT"

echo "A CPU that halts stops, and waits"
run halt "" '600 quit\n' 'nMachineType = 2\nnCpuLevel = 3\n'
check_log halt "CPU halted" "the CPU halted"
if [ "$(grep -c "CPU halted" "$OUT/halt/log.txt")" -le 2 ]; then
    pass "halt: it halted once, and waited"
else
    fail "halt" "it halted again and again (see out/halt/log.txt)"
fi

echo "No ROM"
mkdir -p "$OUT/norom"
(cd "$OUT/norom" && printf '120 quit\n' | BMNEXT_CMDLINE="root=." "$SIM" > log.txt 2>&1)
check_log norom "No ROM for the NeXT Computer" "says there is no ROM"

echo "Web UI requests"
run reboot "" '1200 webui reboot\n1260 quit\n'
check_log reboot "reboot requested" "reboots"
run coldstart "" '1200 webui coldstart\n1260 quit\n'
check_log coldstart "Web UI: restart" "restarts the NeXT"

echo "The power key (Pi 400: Fn+F10) shuts down"
run powerkey "" "$(key $POWER 1200)1260 quit\n"
check_log powerkey "power off requested" "the power key powers off"

echo
if [ $FAILED -eq 0 ]; then
    echo "All tests passed."
else
    echo "$FAILED test(s) failed."
fi
exit $((FAILED > 0))
