#!/bin/sh
set -eu

TAG="${1:-libsmb2-6.2}"
ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
REL_DEST="third_party/libsmb2"
DEST_DIR="$ROOT_DIR/$REL_DEST"
REPO_URL="https://github.com/sahlberg/libsmb2.git"
TEMP_REF="refs/sailvideo-vendor/libsmb2"

if ! git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "ERROR: $ROOT_DIR is not a Git working tree." >&2
    exit 1
fi

echo "Fetching libsmb2 tag $TAG from $REPO_URL"
git -C "$ROOT_DIR" fetch --depth 1 "$REPO_URL" \
    "refs/tags/$TAG:$TEMP_REF"

UPSTREAM_COMMIT=$(git -C "$ROOT_DIR" rev-parse "$TEMP_REF^{commit}")

echo "Replacing the vendored libsmb2 tree in the Git index"
git -C "$ROOT_DIR" rm -r --cached --ignore-unmatch "$REL_DEST" >/dev/null 2>&1 || true
rm -rf "$DEST_DIR"

# Insert the upstream tree directly into SailVideo's index under third_party.
# This deliberately bypasses ignore rules and avoids nested repository metadata.
git -C "$ROOT_DIR" read-tree \
    --prefix="$REL_DEST/" \
    -u \
    "$TEMP_REF^{tree}"

git -C "$ROOT_DIR" update-ref -d "$TEMP_REF"

if ! git -C "$ROOT_DIR" ls-files --error-unmatch \
    "$REL_DEST/CMakeLists.txt" >/dev/null 2>&1; then
    echo "ERROR: $REL_DEST/CMakeLists.txt was not inserted into the Git index." >&2
    exit 1
fi

if ! git -C "$ROOT_DIR" ls-files --error-unmatch \
    "$REL_DEST/COPYING" >/dev/null 2>&1; then
    echo "ERROR: $REL_DEST/COPYING was not inserted into the Git index." >&2
    exit 1
fi

cat <<MESSAGE

libsmb2 is now vendored directly in SailVideo's Git index and working tree.

Upstream:
  $REPO_URL

Tag:
  $TAG

Commit:
  $UPSTREAM_COMMIT

IMPORTANT: the libsmb2 files are staged but not committed yet.

Before committing, this must print a path:
  git ls-files third_party/libsmb2/CMakeLists.txt

Then commit the complete staged tree. After committing, run:
  sh tools/verify-vendored-libsmb2.sh

Do not push if verification reports an error.
MESSAGE
