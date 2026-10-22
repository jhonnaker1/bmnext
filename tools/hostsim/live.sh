#!/bin/bash
#
# Runs BMNeXT in the host simulator in real time, shown in a web page:
#
#   tools/hostsim/live.sh ROM_DIRECTORY [PORT]
#
# then open http://127.0.0.1:PORT/ (default port 8064). The simulator's SD
# card is tools/hostsim/out/live, set up as the Pi's: the NeXT ROMs from
# ROM_DIRECTORY (and its subfolders) in its roms folder, disk images in
# disks. Choosing Reboot (or Shut Down) in the menu starts it again. Ctrl+C
# stops it.
#
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROM_DIR=${1:-}
PORT=${2:-8064}
CARD=$HERE/out/live

make -s -C "$HERE" -j4 || exit 1

mkdir -p "$CARD"/{disks,roms}
if [ -n "$ROM_DIR" ]; then
    find "$ROM_DIR" -iname '*.bin' \( -size 65536c -o -size 131072c \) \
        -exec cp {} "$CARD/roms/" \;
fi

cd "$CARD" || exit 1
while true; do
    BMNEXT_LIVE=$PORT BMNEXT_CMDLINE="root=$CARD ${BMNEXT_CMDLINE:-}" \
        "$HERE/build/bmnext_sim" < /dev/null
    status=$?
    [ $status -ne 0 ] && exit $status
    echo "[live] restarting"
    sleep 1
done
