#!/bin/bash
#
# Fetches Previous's source into third_party/previous.
#
#   tools/update_previous.sh [REVISION]
#
# Previous (https://previous.alternative-system.com/) is kept in Subversion
# on SourceForge, with no official git mirror, so it is not a submodule:
# its trunk is exported at REVISION (default: the one in
# third_party/previous/SVN_REVISION) and committed here, minus what
# BMNeXT must not ship:
#
#   src/*.BIN, src/*.ASM   NeXT's ROMs (and a ROM disassembly): NeXT's
#                          copyright; users bring their own
#   src/dimension/i860*    the NeXTdimension's i860 core, under the MAME
#                          licence (non-commercial, not GPL compatible);
#                          BMNeXT leaves the NeXTdimension out
#   disks/                 blank disk images, not needed
#
# Needs svn (brew install subversion).
#

set -eu

HERE=$(cd "$(dirname "$0")/.." && pwd)
DEST=$HERE/third_party/previous
REV=${1:-$(cat "$DEST/SVN_REVISION" 2>/dev/null || echo HEAD)}
URL=https://svn.code.sf.net/p/previous/code/trunk

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

svn export -q -r "$REV" "$URL" "$TMP/previous"
REV=$(svn info -r "$REV" "$URL" | sed -n 's/^Last Changed Rev: //p')

rm -f "$TMP"/previous/src/*.BIN "$TMP"/previous/src/*.ASM
rm -f "$TMP"/previous/src/dimension/i860*
rm -rf "$TMP/previous/disks"

rm -rf "$DEST"
mv "$TMP/previous" "$DEST"
echo "$REV" > "$DEST/SVN_REVISION"
echo "Previous trunk r$REV in third_party/previous"
