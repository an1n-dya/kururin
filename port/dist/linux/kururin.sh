#!/bin/sh
# Kuru Kuru Kururin, PC port: the launcher of the Linux package (docs/port.md, "The Linux package").
#
#   ./kururin.sh [game options]   checks the 32-bit libraries the game needs, then runs it
#   ./kururin.sh --check          only the check: every library, found or missing
#
# kururin_pc is a 32-bit (i386) program. libSDL2 comes with it (next to it, found through its
# RUNPATH $ORIGIN); the rest is the system's, 32-bit: glibc to start at all, X11 for the window,
# and what SDL loads when it needs it (sound, OpenGL, gamepads). Those cannot be shipped (they
# belong to the system's display and sound servers and graphics drivers), so this script looks
# for them and, if some are missing, prints the command that installs them on this distribution
# (Debian/Ubuntu, Fedora, Arch; all three when it does not know the system). The save
# (kururin.sav), the settings (kururin.ini) and kururin.log are next to kururin_pc, as on Windows.
# KURURIN_NO_CHECK=1 skips the check.

self=$(readlink -f "$0" 2>/dev/null) || self=$0
dir=$(cd "$(dirname "$self")" && pwd) || exit 1
game=$dir/kururin_pc

check_only=0
headless=0
for a in "$@"; do
    case $a in
    --check) check_only=1 ;;
    --headless) headless=1 ;;
    esac
done

say() { printf '%s\n' "$*" >&2; }

# A fatal problem: said on the terminal, else in a dialog (started from a file manager), and
# written to kururin.log like the game's own errors
fail() {
    say "kururin.sh: $1"
    { printf 'kururin.sh: %s\n' "$1" > "$dir/kururin.log"; } 2>/dev/null
    if [ "$check_only" = 0 ] && [ ! -t 2 ] && [ -n "$DISPLAY$WAYLAND_DISPLAY" ]; then
        if command -v zenity > /dev/null 2>&1; then
            zenity --error --no-markup --title "Kuru Kuru Kururin" --text "$1" 2> /dev/null
        elif command -v kdialog > /dev/null 2>&1; then
            kdialog --title "Kuru Kuru Kururin" --error "$1" 2> /dev/null
        elif command -v xmessage > /dev/null 2>&1; then
            printf '%s\n' "$1" | xmessage -center -file - 2> /dev/null
        fi
    fi
    exit 1
}

[ "$KURURIN_NO_CHECK" = 1 ] && [ "$check_only" = 0 ] && exec "$game" "$@"

case $(uname -m) in
x86_64 | i?86) ;;
*) fail "kururin_pc is a program for x86 PCs (32-bit, i386); this machine is $(uname -m)." ;;
esac
[ -f "$game" ] || fail "$game is missing: unpack the whole archive."

# --- Which distribution, and its package names -------------------------------------------------

family=
if [ -r /etc/os-release ]; then
    ids=$(. /etc/os-release; echo "$ID $ID_LIKE")
    for id in $ids; do
        case $id in
        debian | ubuntu) family=${family:-debian} ;;
        fedora | rhel | centos) family=${family:-fedora} ;;
        arch) family=${family:-arch} ;;
        esac
    done
fi
if [ -z "$family" ]; then
    if command -v apt-get > /dev/null 2>&1; then family=debian
    elif command -v dnf > /dev/null 2>&1; then family=fedora
    elif command -v pacman > /dev/null 2>&1; then family=arch
    fi
fi

# The package that holds a library, 32-bit: package LIBRARY debian|fedora|arch
package() {
    case $2 in
    debian) case $1 in
        ld-linux.so.2 | libc.so.6 | libm.so.6) echo libc6:i386 ;;
        libX11.so.6) echo libx11-6:i386 ;;
        libXext.so.6) echo libxext6:i386 ;;
        libXcursor.so.1) echo libxcursor1:i386 ;;
        libXi.so.6) echo libxi6:i386 ;;
        libXrandr.so.2) echo libxrandr2:i386 ;;
        libXss.so.1) echo libxss1:i386 ;;
        libXfixes.so.3) echo libxfixes3:i386 ;;
        libGL.so.1) echo libgl1:i386 libgl1-mesa-dri:i386 ;;
        libpulse.so.0) echo libpulse0:i386 ;;
        libasound.so.2) echo libasound2:i386 ;;
        libudev.so.1) echo libudev1:i386 ;;
        libdbus-1.so.3) echo libdbus-1-3:i386 ;;
        esac ;;
    fedora) case $1 in
        ld-linux.so.2 | libc.so.6 | libm.so.6) echo glibc.i686 ;;
        libX11.so.6) echo libX11.i686 ;;
        libXext.so.6) echo libXext.i686 ;;
        libXcursor.so.1) echo libXcursor.i686 ;;
        libXi.so.6) echo libXi.i686 ;;
        libXrandr.so.2) echo libXrandr.i686 ;;
        libXss.so.1) echo libXScrnSaver.i686 ;;
        libXfixes.so.3) echo libXfixes.i686 ;;
        libGL.so.1) echo mesa-libGL.i686 mesa-dri-drivers.i686 ;;
        libpulse.so.0) echo pulseaudio-libs.i686 ;;
        libasound.so.2) echo alsa-lib.i686 ;;
        libudev.so.1) echo systemd-libs.i686 ;;
        libdbus-1.so.3) echo dbus-libs.i686 ;;
        esac ;;
    arch) case $1 in
        ld-linux.so.2 | libc.so.6 | libm.so.6) echo lib32-glibc ;;
        libX11.so.6) echo lib32-libx11 ;;
        libXext.so.6) echo lib32-libxext ;;
        libXcursor.so.1) echo lib32-libxcursor ;;
        libXi.so.6) echo lib32-libxi ;;
        libXrandr.so.2) echo lib32-libxrandr ;;
        libXss.so.1) echo lib32-libxss ;;
        libXfixes.so.3) echo lib32-libxfixes ;;
        libGL.so.1) echo lib32-mesa ;;
        libpulse.so.0) echo lib32-libpulse ;;
        libasound.so.2) echo lib32-alsa-lib ;;
        libudev.so.1) echo lib32-systemd ;;
        libdbus-1.so.3) echo lib32-dbus ;;
        esac ;;
    esac
}

# The install command for these libraries: install_command debian|fedora|arch LIBRARY...
install_command() {
    fam=$1
    shift
    pkgs=
    for lib in "$@"; do
        for p in $(package "$lib" "$fam"); do
            case " $pkgs " in *" $p "*) ;; *) pkgs="$pkgs $p" ;; esac
        done
    done
    case $fam in
    debian)
        if [ "$(dpkg --print-architecture 2>/dev/null)" != i386 ] &&
            ! dpkg --print-foreign-architectures 2>/dev/null | grep -qx i386; then
            echo "sudo dpkg --add-architecture i386 && sudo apt update && sudo apt install$pkgs"
        else
            echo "sudo apt update && sudo apt install$pkgs"
        fi ;;
    fedora) echo "sudo dnf install$pkgs" ;;
    arch)
        if [ -r /etc/pacman.conf ] && ! grep -q '^\[multilib\]' /etc/pacman.conf; then
            echo "sudo sed -i '/^#\[multilib\]/,/^#Include/ s/^#//' /etc/pacman.conf && sudo pacman -Syu --needed$pkgs"
        else
            echo "sudo pacman -S --needed$pkgs"
        fi ;;
    esac
}

# The commands, for this system or (unknown) all three
install_help() {
    if [ -n "$family" ]; then
        printf '  %s\n' "$(install_command "$family" "$@")"
    else
        printf 'Debian, Ubuntu:\n  %s\n' "$(install_command debian "$@")"
        printf 'Fedora:\n  %s\n' "$(install_command fedora "$@")"
        printf 'Arch (the multilib repository):\n  %s\n' "$(install_command arch "$@")"
    fi
}

# --- Finding 32-bit libraries ------------------------------------------------------------------

ldconfig=
for l in /sbin/ldconfig /usr/sbin/ldconfig /usr/bin/ldconfig; do
    [ -x "$l" ] && ldconfig=$l && break
done
cache=$([ -n "$ldconfig" ] && "$ldconfig" -p 2>/dev/null)

# Whether a file is a 32-bit i386 ELF file (class 1 at byte 4, machine 3 at byte 18)
is_i386() {
    [ "$(od -An -tx1 -j4 -N1 "$1" 2>/dev/null | tr -d ' ')" = 01 ] &&
        [ "$(od -An -tx1 -j18 -N1 "$1" 2>/dev/null | tr -d ' ')" = 03 ]
}

# Whether the 32-bit library is installed: in the loader's cache as a 32-bit one, else in one of
# the places the distributions put them
have() {
    printf '%s\n' "$cache" | grep -Eq "^[[:space:]]*$1 \\(libc6(\\)|, )" && return 0
    for d in /lib/i386-linux-gnu /usr/lib/i386-linux-gnu /usr/lib32 /lib32 /usr/lib /lib; do
        [ -e "$d/$1" ] && is_i386 "$d/$1" && return 0
    done
    return 1
}

# --- The check ---------------------------------------------------------------------------------

report() { [ "$check_only" = 1 ] && printf '  %-22s %s\n' "$1" "$2"; }

# 1. To start at all: the 32-bit loader and what kururin_pc and libSDL2 link (ldd)
[ "$check_only" = 1 ] && echo "To start (kururin_pc and libSDL2, from ldd):"
core=
if [ ! -e /lib/ld-linux.so.2 ]; then
    core="ld-linux.so.2 libc.so.6 libm.so.6"
    report ld-linux.so.2 "missing: this system cannot run 32-bit programs yet"
else
    if command -v ldd > /dev/null 2>&1; then
        list=$(ldd "$game" "$dir/libSDL2-2.0.so.0" 2>&1)
    else
        list=$(/lib/ld-linux.so.2 --list "$game" 2>&1)
    fi
    for lib in $(printf '%s\n' "$list" | sed -n 's/^[[:space:]]*\([^[:space:]]*\) => not found.*/\1/p' | sort -u); do
        case $lib in
        libSDL2-2.0.so.0) fail "libSDL2-2.0.so.0 is missing next to kururin_pc: unpack the whole archive." ;;
        esac
        core="$core $lib"
        report "$lib" missing
    done
    if [ -z "$core" ]; then
        report "glibc (32-bit)" found
        report libSDL2-2.0.so.0 "found (next to kururin_pc)"
    fi
fi
if [ -n "$core" ] && [ "$headless" = 1 ] && [ "$check_only" = 0 ]; then
    fail "kururin_pc cannot start: this system has no 32-bit glibc (or misses part of it).
Install it with:
$(install_help $core)"
fi
if [ "$headless" = 1 ] && [ "$check_only" = 0 ]; then
    exec "$game" "$@"
fi

# 2. The window: X11's libraries (SDL2 uses X11, also on a Wayland desktop through XWayland), or
# Wayland's when there is no X11
[ "$check_only" = 1 ] && echo "The window:"
window=
for lib in libX11.so.6 libXext.so.6; do
    if have $lib; then report $lib found; else report $lib missing; window="$window $lib"; fi
done
if [ -n "$window" ] && [ -n "$WAYLAND_DISPLAY" ]; then
    wayland=yes
    for lib in libwayland-client.so.0 libwayland-cursor.so.0 libwayland-egl.so.1 libxkbcommon.so.0 libEGL.so.1; do
        have $lib || wayland=
    done
    [ -n "$wayland" ] && window= && report "Wayland's libraries" "found (used instead of X11)"
fi

# 3. Recommended: the game runs without them, with less
[ "$check_only" = 1 ] && echo "Recommended:"
extra=
effects=
for entry in \
    "libpulse.so.0:sound through PulseAudio or PipeWire" \
    "libasound.so.2:sound through ALSA" \
    "libGL.so.1:OpenGL (without it the picture is scaled in software)" \
    "libXcursor.so.1:X11 extras (cursor, input, screen modes, screensaver)" \
    "libXi.so.6:" "libXrandr.so.2:" "libXss.so.1:" "libXfixes.so.3:" \
    "libudev.so.1:gamepads plugged in while the game runs" \
    "libdbus-1.so.3:keeping the screensaver off"; do
    lib=${entry%%:*}
    what=${entry#*:}
    if have "$lib"; then
        report "$lib" found
    else
        report "$lib" "missing${what:+: $what}"
        extra="$extra $lib"
        [ -n "$what" ] && effects="$effects
  - $what"
    fi
done
if ! have libpulse.so.0 && ! have libasound.so.2; then
    effects="$effects
  (the game runs without sound)"
fi

if [ "$check_only" = 1 ]; then
    if [ -n "$core$window$extra" ]; then
        echo "To install what is missing:"
        install_help $core $window $extra
    else
        echo "Everything is installed."
    fi
    [ -n "$core$window" ] && exit 1
    exit 0
fi

# One command for everything missing, also when glibc is (a bare 64-bit system)
if [ -n "$core" ]; then
    fail "kururin_pc cannot start: this system has no 32-bit glibc (or misses part of it).
Install it, with the libraries for the window, sound and OpenGL, with:
$(install_help $core $window $extra)
(./kururin.sh --check lists every library.)"
fi

if [ -n "$window" ]; then
    fail "kururin_pc cannot open its window: the 32-bit X11 libraries are missing.
Install them (and the recommended ones) with:
$(install_help $window $extra)
(./kururin.sh --check lists every library.)"
fi
if [ -n "$extra" ]; then
    say "kururin.sh: some recommended 32-bit libraries are missing; the game runs without them:$effects"
    say "Install them with:"
    install_help $extra >&2
    say "(KURURIN_NO_CHECK=1 ./kururin.sh starts the game without this check.)"
fi
[ -w "$dir" ] || say "kururin.sh: $dir is not writable: the game cannot save there (copy the folder somewhere you own)."
exec "$game" "$@"
