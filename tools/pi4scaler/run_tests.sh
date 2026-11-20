#!/bin/bash
#
# Tests the shared Pi 4/5 ARM smooth scaling and CRT shader (src/av/scaler.*)
# and how a frame is shared between cores (src/av/frameshare.h) on the PC.
# The pictures go to tools/pi4scaler/out/.
#
#   tools/pi4scaler/run_tests.sh
#

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
make -s -C "$HERE"
rm -rf "$HERE/out"
mkdir -p "$HERE/out"
"$HERE/build/pi4scaler_test" "$HERE/out"
echo
# ThreadSanitizer stops on some newer Linux kernels' address layout; run it
# without address space randomisation where setarch is available. Where it
# can't run at all (a kernel with 47-bit addresses, as on a Pi 5, which it
# doesn't support: GitHub issue #1), the test runs without it.
run_share() {
    if command -v setarch > /dev/null
    then
        setarch "$(uname -m)" -R "$@"
    else
        "$@"
    fi
}
LOG="$HERE/out/share_test.log"
if run_share "$HERE/build/share_test" > "$LOG" 2>&1
then
    cat "$LOG"
elif grep -q "ThreadSanitizer: unsupported VMA range" "$LOG"
then
    echo "ThreadSanitizer can't run on this machine's address layout:"
    echo "the frame sharing test runs without it."
    run_share "$HERE/build/share_test_plain"
else
    cat "$LOG"
    exit 1
fi
