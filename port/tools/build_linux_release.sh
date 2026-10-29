#!/bin/sh
# Builds the Linux release package dist/kururin_pc_linux_x86.tar.gz (docs/port.md, "The Linux
# package") from a clean tree, in a Docker container of an older distribution: a program built
# against glibc 2.39 asks for glibc 2.38's symbol versions (__isoc23_strtol, strlcpy, ...) and
# would not start on Debian 12 (2.36) or Ubuntu 22.04 (2.35). Built on Ubuntu 22.04 it needs
# glibc 2.35 at most (port/tools/package_linux.py checks that).
#
# usage: port/tools/build_linux_release.sh [IMAGE]       (default ubuntu:22.04)
#
# It clones this repository's HEAD (uncommitted changes are not in it) into a temporary
# directory, copies baserom.gba there (it is not in git; package_linux.py checks that none of it
# is packaged), runs `make -C port dist-linux` there inside IMAGE (installing gcc-multilib, cmake,
# python3.11 and the 32-bit development headers SDL looks for, libsdl2-dev:i386), and copies the
# package to dist/. DOCKER_ARGS adds arguments to docker run (a proxy, --network host); with
# APT_HTTPS=1 apt uses https mirrors, APT_OPTS adds apt options (e.g. -oAcquire::https::CaInfo=FILE).
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
image=${1:-ubuntu:22.04}
work=$(mktemp -d "${TMPDIR:-/tmp}/kururin-linux-release.XXXXXX")
trap 'rm -rf "$work"' EXIT

[ -f "$root/baserom.gba" ] || { echo "build_linux_release.sh: $root/baserom.gba is missing (the package's ROM check needs it)" >&2; exit 1; }
git -C "$root" diff --quiet HEAD -- || echo "build_linux_release.sh: uncommitted changes are left out (building HEAD)" >&2
git clone -q --no-local "$root" "$work/src"
cp "$root/baserom.gba" "$work/src/"
echo "building $(git -C "$work/src" rev-parse --short HEAD) in $image"
# SDL's source, if this checkout downloaded it already (its SHA-256 is checked again)
for t in "$root"/port/build/sdl2-linux/SDL2-*.tar.gz; do
    [ -f "$t" ] && mkdir -p "$work/src/port/build/sdl2-linux" && cp "$t" "$work/src/port/build/sdl2-linux/"
done

# shellcheck disable=SC2086
docker run --rm ${DOCKER_ARGS:-} -v "$work:/work" -e APT_HTTPS="${APT_HTTPS:-}" -e APT_OPTS="${APT_OPTS:-}" \
    -e HOST_IDS="$(id -u):$(id -g)" "$image" sh -euc '
export DEBIAN_FRONTEND=noninteractive
[ -n "$APT_HTTPS" ] && sed -i s,http://,https://,g /etc/apt/sources.list
dpkg --add-architecture i386
apt-get $APT_OPTS update -qq
echo "installing the build dependencies (apt.log)"
apt-get $APT_OPTS install -y -qq --no-install-recommends gcc-multilib make cmake ninja-build python3.11 \
    git curl ca-certificates file binutils pkg-config libsdl2-dev:i386 > /work/apt.log 2>&1 ||
    { tail -20 /work/apt.log; exit 1; }
ln -sf /usr/bin/python3.11 /usr/local/bin/python3  # the port build reads kururin.toml with tomllib
gcc --version | head -1
ldd --version | head -1
make -C /work/src/port dist-linux
chown -R "$HOST_IDS" /work
'
mkdir -p "$root/dist"
cp "$work/src/dist/kururin_pc_linux_x86.tar.gz" "$root/dist/"
sha256sum "$root/dist/kururin_pc_linux_x86.tar.gz"
