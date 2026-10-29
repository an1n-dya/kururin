#!/bin/sh
# Builds SDL 2.32.10 for the PC port's Linux package (docs/port.md, "The Linux package"):
# make -C port dist-linux. 32-bit (gcc -m32), shared, Release, with every system backend loaded
# at run time (dlopen): X11, Wayland, KMS/DRM, ALSA, PulseAudio, sndio, udev, D-Bus, libdecor
# (not PipeWire's own API: PipeWire desktops run its PulseAudio server; not libsamplerate: SDL's
# own resampler). So the library needs nothing but libc and libm to load, and uses
# whatever video and sound the player's system has; the distribution's own libSDL2 links most of
# them directly and would need all of them installed.
#
# usage: port/tools/build_sdl2_linux.sh DEST_DIR
# Writes DEST_DIR/prefix/ (include/SDL2, lib/libSDL2-2.0.so.0, ...); the source's SHA-256 is pinned.
# Needs cmake, ninja, gcc-multilib and the 32-bit development headers SDL looks for (Ubuntu:
# libsdl2-dev:i386 pulls them in). port/tools/build_linux_release.sh runs it on Ubuntu 22.04.
set -eu
VERSION=2.32.10
SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
URL=https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz

dest=$1
mkdir -p "$dest"
tarball=$dest/SDL2-$VERSION.tar.gz
if [ ! -f "$tarball" ]; then
    echo "downloading SDL2 $VERSION (source)"
    curl -fsSL -o "$tarball.part" "$URL"
    mv "$tarball.part" "$tarball"
fi
echo "$SHA256  $tarball" | sha256sum -c --quiet - || { echo "build_sdl2_linux.sh: $tarball: wrong SHA-256" >&2; exit 1; }
rm -rf "$dest/SDL2-$VERSION" "$dest/build" "$dest/prefix"
tar -xzf "$tarball" -C "$dest"
echo "building SDL2 $VERSION (32-bit, backends loaded at run time)"
PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig \
cmake -S "$dest/SDL2-$VERSION" -B "$dest/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-m32" -DCMAKE_SHARED_LINKER_FLAGS="-m32" -DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu \
    -DCMAKE_INSTALL_PREFIX="$dest/prefix" -DCMAKE_INSTALL_LIBDIR=lib \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_RPATH=OFF \
    -DSDL_X11_SHARED=ON -DSDL_WAYLAND_SHARED=ON -DSDL_KMSDRM_SHARED=ON -DSDL_ALSA_SHARED=ON \
    -DSDL_PULSEAUDIO_SHARED=ON -DSDL_PIPEWIRE=OFF -DSDL_SNDIO_SHARED=ON -DSDL_JACK=OFF \
    -DSDL_LIBSAMPLERATE=OFF -DSDL_WAYLAND_LIBDECOR_SHARED=ON > "$dest/cmake.log"
ninja -C "$dest/build" > "$dest/ninja.log"
ninja -C "$dest/build" install > /dev/null
