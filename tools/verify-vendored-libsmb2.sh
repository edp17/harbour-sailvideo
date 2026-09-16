#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REL_DEST="third_party/libsmb2"
DEST_DIR="$ROOT_DIR/$REL_DEST"

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

[ -f "$DEST_DIR/CMakeLists.txt" ] \
    || fail "$REL_DEST/CMakeLists.txt is missing from the working tree."

[ -f "$DEST_DIR/COPYING" ] \
    || fail "$REL_DEST/COPYING is missing."

[ -f "$DEST_DIR/LICENCE-LGPL-2.1.txt" ] \
    || fail "$REL_DEST/LICENCE-LGPL-2.1.txt is missing."

[ ! -e "$DEST_DIR/.git" ] \
    || fail "$REL_DEST contains nested Git metadata."

git -C "$ROOT_DIR" ls-files --error-unmatch \
    "$REL_DEST/CMakeLists.txt" >/dev/null 2>&1 \
    || fail "$REL_DEST/CMakeLists.txt exists locally but is not tracked."

git -C "$ROOT_DIR" cat-file -e \
    "HEAD:$REL_DEST/CMakeLists.txt" 2>/dev/null \
    || fail "$REL_DEST/CMakeLists.txt is not present in the current commit."

git -C "$ROOT_DIR" cat-file -e \
    "HEAD:$REL_DEST/COPYING" 2>/dev/null \
    || fail "$REL_DEST/COPYING is not present in the current commit."

COUNT=$(git -C "$ROOT_DIR" ls-tree -r --name-only HEAD "$REL_DEST" | wc -l)
[ "$COUNT" -gt 20 ]     || fail "Only $COUNT files are stored under $REL_DEST in HEAD; the full vendored tree is missing."

echo "OK: vendored libsmb2 is present in the working tree and current Git commit."
echo "Tracked libsmb2 files in HEAD: $COUNT"
echo "A normal clone of this commit will contain $REL_DEST/CMakeLists.txt."
