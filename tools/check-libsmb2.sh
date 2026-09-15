#!/bin/sh
set -eu

echo "SailVideo libsmb2 packaging check"
echo

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ -f "$PROJECT_ROOT/third_party/libsmb2/CMakeLists.txt" ]; then
    echo "OK: third_party/libsmb2 source is present."
else
    echo "MISSING: third_party/libsmb2/CMakeLists.txt"
    echo "Run: sh tools/import-libsmb2.sh"
fi

echo
echo "Build-tree libsmb2 files:"
find "$PROJECT_ROOT" -path '*/harbour-sailvideo/*' -name 'libsmb2.so*' -print 2>/dev/null || true
find "$PROJECT_ROOT" -path '*/third_party/libsmb2/*' -name 'libsmb2.so*' -print 2>/dev/null || true

echo
echo "Installed device libsmb2 files:"
find /usr/lib /usr/lib64 -path '*harbour-sailvideo*' -name 'libsmb2.so*' -print 2>/dev/null || true

echo
echo "RPM payload libsmb2 files in current directory, if any:"
for rpm in ./*.rpm RPMS/*/*.rpm; do
    [ -f "$rpm" ] || continue
    echo "--- $rpm"
    rpm -qlp "$rpm" 2>/dev/null | grep 'harbour-sailvideo.*/libsmb2\.so' || true
done
