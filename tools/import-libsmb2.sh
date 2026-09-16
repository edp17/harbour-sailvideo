#!/bin/sh
set -eu

TAG="${1:-libsmb2-6.2}"
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DEST_DIR="$ROOT_DIR/third_party/libsmb2"
REPO_URL="https://github.com/sahlberg/libsmb2.git"
REL_DEST="third_party/libsmb2"

stage_and_verify()
{
    if ! git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "ERROR: $ROOT_DIR is not a Git working tree." >&2
        exit 1
    fi

    if [ -e "$DEST_DIR/.git" ]; then
        echo "ERROR: $DEST_DIR still contains nested Git metadata." >&2
        exit 1
    fi

    git -C "$ROOT_DIR" add -f "$REL_DEST"

    if ! git -C "$ROOT_DIR" ls-files --error-unmatch \
        "$REL_DEST/CMakeLists.txt" >/dev/null 2>&1; then
        echo "ERROR: libsmb2/CMakeLists.txt exists locally but is not staged/tracked." >&2
        exit 1
    fi

    if ! git -C "$ROOT_DIR" ls-files --error-unmatch \
        "$REL_DEST/COPYING" >/dev/null 2>&1; then
        echo "ERROR: libsmb2 licence files are not staged/tracked." >&2
        exit 1
    fi
}

if [ -f "$DEST_DIR/CMakeLists.txt" ]; then
    echo "libsmb2 source already exists in $DEST_DIR"
    echo "Staging and verifying the vendored tree."
    stage_and_verify
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

stage_and_verify

cat <<MESSAGE

libsmb2 is now vendored and staged in:
  $DEST_DIR

Upstream tag:
  $TAG

Upstream commit:
  $UPSTREAM_COMMIT

Before pushing, commit the staged vendored source and run:
  sh tools/verify-vendored-libsmb2.sh

Then push the commit normally.
MESSAGE
