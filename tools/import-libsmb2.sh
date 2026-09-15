#!/bin/sh
set -eu

TAG="${1:-libsmb2-6.2}"
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST_DIR="$ROOT_DIR/third_party/libsmb2"
REPO_URL="https://github.com/sahlberg/libsmb2.git"

if [ -d "$DEST_DIR/.git" ]; then
    echo "Updating existing libsmb2 checkout in $DEST_DIR"
    git -C "$DEST_DIR" fetch --tags origin
    git -C "$DEST_DIR" checkout "$TAG"
elif [ -f "$DEST_DIR/CMakeLists.txt" ]; then
    echo "libsmb2 source already exists in $DEST_DIR"
    echo "Leaving it untouched. Remove the directory first if you want to re-import."
else
    rm -rf "$DEST_DIR"
    mkdir -p "$(dirname -- "$DEST_DIR")"
    echo "Cloning libsmb2 $TAG into $DEST_DIR"
    git clone --depth 1 --branch "$TAG" "$REPO_URL" "$DEST_DIR"
fi

if [ ! -f "$DEST_DIR/CMakeLists.txt" ]; then
    echo "ERROR: libsmb2 import did not create $DEST_DIR/CMakeLists.txt" >&2
    exit 1
fi

if [ ! -f "$DEST_DIR/COPYING" ] || [ ! -f "$DEST_DIR/LICENCE-LGPL-2.1.txt" ]; then
    echo "ERROR: libsmb2 licence files are missing after import." >&2
    exit 1
fi

cat <<MESSAGE

libsmb2 is ready.

Next build SailVideo normally. CMake should print:
  Building bundled app-private libsmb2 from third_party/libsmb2

The RPM should then contain an app-private library under:
  /usr/lib*/harbour-sailvideo/libsmb2.so*
MESSAGE
