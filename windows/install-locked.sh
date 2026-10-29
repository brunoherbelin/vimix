#!/bin/sh
# Install the exact package versions of windows/packages.lock in the MSYS2
# toolchain (run by windows/build.ps1 -Locked, in the UCRT64 shell).
#
# The packages are downloaded with their signature into .win-toolchain/pkgcache,
# checked against the MSYS2 keyring, then installed from disk (pacman does not
# check the signature of local files by default, and mishandles some versioned
# urls, e.g. with '~').
#
# usage: sh windows/install-locked.sh <packages.lock> <repository url>
set -e

lock="$1"
repo="$2"
cache=.win-toolchain/pkgcache
mkdir -p "$cache"

# file name of each package: name-version-any.pkg.tar.zst
files=$(grep '^mingw-w64-' "$lock" | tr -d '\r' | while read -r name version; do
    echo "$name-$version-any.pkg.tar.zst"
done)

echo "Downloading and checking $(echo "$files" | wc -l) locked packages ..."
for f in $files; do
    for x in "$f" "$f.sig"; do
        # each file is downloaded as .part first, so an interrupted download is not kept
        if [ ! -s "$cache/$x" ]; then
            curl -fsSL --retry 5 --retry-all-errors -o "$cache/$x.part" "$repo/$x"
            mv "$cache/$x.part" "$cache/$x"
        fi
    done
    # (gpgv with the keyring of pacman: pacman-key --verify needs admin rights)
    if ! gpgv --keyring /etc/pacman.d/gnupg/pubring.gpg "$cache/$f.sig" "$cache/$f" >/dev/null 2>&1; then
        echo "Invalid signature: $f"
        rm -f "$cache/$f" "$cache/$f.sig"
        exit 1
    fi
done

echo "Installing locked packages ..."
(cd "$cache" && pacman -U --noconfirm --needed $files)
