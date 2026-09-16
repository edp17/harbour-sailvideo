#!/bin/sh
set -eu

TAG="${1:-libsmb2-6.2}"
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST_DIR="$ROOT_DIR/third_party/libsmb2"
REPO_URL="https://github.com/sahlberg/libsmb2.git"

if [ -f "$DEST_DIR/CMakeLists.txt" ]; then
    echo "libsmb2 source already exists in $DEST_DIR"
    echo "Leaving it untouched. Remove the directory first if you want to re-vendor it."
    exit 0
fi

TMP_BASE="${TMPDIR:-/tmp}"
TMP_DIR=$(mktemp -d "$TMP_BASE/sailvideo-libsmb2.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT HUP INT TERM

echo "Cloning libsmb2 $TAG from $REPO_URL"
git clone --depth 1 --branch "$TAG" "$REPO_URL" "$TMP_DIR/libsmb2"

UPSTREAM_COMMIT=$(git -C "$TMP_DIR/libsmb2" rev-parse HEAD)

if [ ! -f "$TMP_DIR/libsmb2/CMakeLists.txt" ]; then
    echo "ERROR: libsmb2 checkout does not contain CMakeLists.txt" >&2
    exit 1
fi

if [ ! -f "$TMP_DIR/libsmb2/COPYING" ] || [ ! -f "$TMP_DIR/libsmb2/LICENCE-LGPL-2.1.txt" ]; then
    echo "ERROR: libsmb2 licence files are missing from the checkout." >&2
    exit 1
fi

rm -rf "$TMP_DIR/libsmb2/.git"
rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"
cp -a "$TMP_DIR/libsmb2/." "$DEST_DIR/"

cat > "$DEST_DIR/SAILVIDEO_VENDOR.txt" <<EOF
Upstream: $REPO_URL
Tag: $TAG
Commit: $UPSTREAM_COMMIT

This directory is vendored into the SailVideo source repository so a normal
git clone contains everything required for SMB-enabled builds.
EOF

cat <<MESSAGE

libsmb2 is now vendored into:
  $DEST_DIR

Upstream tag:
  $TAG

Upstream commit:
  $UPSTREAM_COMMIT

The copied tree is an ordinary part of the SailVideo repository, not a nested
Git repository or submodule.

Commit it with:
  git add third_party/libsmb2 tools/import-libsmb2.sh
  git commit -m "Vendor libsmb2 $TAG for reproducible builds"

After it is pushed, a normal clone of harbour-sailvideo will contain libsmb2
and can build SMB/NAS support without running this script.
MESSAGE
