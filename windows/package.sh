#!/bin/sh
# Make a portable vimix package for Windows, in the MSYS2 UCRT64 shell:
#   sh windows/package.sh <build dir> <dist dir>
#
# Layout (GStreamer finds its plugins relative to its own DLL, in ../lib):
#   bin/vimix.exe, bin/*.dll                       vimix and the DLLs it needs
#   lib/gstreamer-1.0/*.dll                        GStreamer plugins
#   libexec/gstreamer-1.0/gst-plugin-scanner.exe   plugin registry scanner
# All DLLs needed by vimix, the plugins and the scanner are taken from the
# UCRT64 environment; system DLLs of Windows are left out.
set -e

BUILD=${1:-build-win}
DIST=${2:-dist}
PREFIX=$(cygpath -m /ucrt64)

rm -rf "$DIST"
mkdir -p "$DIST/bin" "$DIST/lib/gstreamer-1.0" "$DIST/libexec/gstreamer-1.0"

cp "$BUILD/src/vimix.exe" "$DIST/bin/"
cp /ucrt64/lib/gstreamer-1.0/*.dll "$DIST/lib/gstreamer-1.0/"
cp /ucrt64/libexec/gstreamer-1.0/gst-plugin-scanner.exe "$DIST/libexec/gstreamer-1.0/"

# DLLs of the UCRT64 environment needed by all of the above (recursively)
deps=$(mktemp)
for f in "$DIST/bin/vimix.exe" "$DIST"/lib/gstreamer-1.0/*.dll "$DIST/libexec/gstreamer-1.0/gst-plugin-scanner.exe"; do
    ntldd -R "$f" 2>/dev/null
done | sed -n 's/.* => \(.*\.dll\) (0x.*/\1/p' | tr '\\' '/' | grep -i "^$PREFIX/bin/" | sort -u > "$deps"

while read -r dll; do
    cp "$dll" "$DIST/bin/"
done < "$deps"
echo "Copied $(wc -l < "$deps") DLLs"
rm -f "$deps"

cp COPYING.txt "$DIST/"
cp windows/README-windows.txt "$DIST/README.txt"

echo "Package size: $(du -sh "$DIST" | cut -f1)"
