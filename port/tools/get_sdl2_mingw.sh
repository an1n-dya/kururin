#!/bin/sh
# Downloads SDL2's official MinGW development package (the 32-bit i686-w64-mingw32 headers,
# import library and SDL2.dll) for the PC port's Windows build: make -C port windows.
#
# usage: port/tools/get_sdl2_mingw.sh DEST_DIR
# Writes DEST_DIR/SDL2-<version>/ (the package as released); the SHA-256 is pinned.
set -eu
VERSION=2.32.10
SHA256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
URL=https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-devel-$VERSION-mingw.tar.gz

dest=$1
mkdir -p "$dest"
tarball=$dest/SDL2-devel-$VERSION-mingw.tar.gz
if [ ! -f "$tarball" ]; then
    echo "downloading SDL2 $VERSION (MinGW)"
    curl -fsSL -o "$tarball.part" "$URL"
    mv "$tarball.part" "$tarball"
fi
echo "$SHA256  $tarball" | sha256sum -c --quiet - || { echo "get_sdl2_mingw.sh: $tarball: wrong SHA-256" >&2; exit 1; }
tar -xzf "$tarball" -C "$dest"
touch "$dest/SDL2-$VERSION/i686-w64-mingw32/lib/libSDL2.dll.a"
