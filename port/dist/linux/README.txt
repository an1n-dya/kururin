Kuru Kuru Kururin - PC port for Linux
=====================================

A 32-bit (i386) program for 32- and 64-bit x86 Linux with glibc 2.34 or newer: Debian 12,
Ubuntu 22.04, Fedora 35, RHEL 9 or later, Arch and the other current distributions.

This is the game's own code, decompiled to C and compiled for the PC, with a small layer
standing in for the Game Boy Advance's hardware. It contains none of the cartridge's data:
no graphics, no music, no levels. Those come from your own copy of the cartridge.

    kururin.sh          the launcher: checks the libraries, then starts the game
    kururin_pc          the game
    libSDL2-2.0.so.0    SDL 2.32.10 (window, sound, input), found next to kururin_pc
    LICENSE-*.txt, README-SDL.txt, source/   licences, and the source the MPL asks for

Getting started
---------------

1. Unpack the archive somewhere you can write to (your home directory):

       tar -xzf kururin_pc_linux_x86.tar.gz

2. Make a dump of your own Kuru Kuru Kururin cartridge (Europe, AKRP), name it

       baserom.gba

   and put it in the kururin_pc folder, next to kururin_pc. It must be exactly that ROM:
   4 194 304 bytes, SHA-1 6025ef597db2684ae064ffe22b4fd1d37941f887. The game checks it at
   start-up and says so if the file is missing or different.

3. Install the 32-bit libraries (once; next section), then start the game:

       cd kururin_pc
       ./kururin.sh

The 32-bit libraries
--------------------

A 64-bit Linux runs 32-bit programs once it has the 32-bit versions of the system libraries
they use. SDL2 comes with the game; these are the system's, and cannot be shipped with it
(they belong to the system's graphics drivers and sound and display servers):

    glibc (libc, libm)        to start at all
    X11 (libX11, libXext)     the window (on a Wayland desktop too, through XWayland)
    PulseAudio's libpulse     sound (also on PipeWire desktops); or ALSA's libasound
    Mesa's OpenGL (libGL)     scaling the picture on the graphics card (without it: in
                              software, which works too)
    libXcursor, libXi, libXrandr, libXss, libXfixes   X11 extras
    libudev                   gamepads plugged in while the game runs
    libdbus                   keeping the screensaver off

kururin.sh looks for them each time it starts. If some are missing, it says which and prints
the command that installs them on your distribution; ./kururin.sh --check lists every
library. The commands for everything at once:

Debian, Ubuntu, Linux Mint and the like (the first two lines once, to enable 32-bit packages):

    sudo dpkg --add-architecture i386
    sudo apt update
    sudo apt install libc6:i386 libx11-6:i386 libxext6:i386 libpulse0:i386 libasound2:i386 \
        libgl1:i386 libgl1-mesa-dri:i386 libxcursor1:i386 libxi6:i386 libxrandr2:i386 \
        libxss1:i386 libxfixes3:i386 libudev1:i386 libdbus-1-3:i386

Fedora (and RHEL, Rocky, Alma with their AppStream repository):

    sudo dnf install glibc.i686 libX11.i686 libXext.i686 pulseaudio-libs.i686 alsa-lib.i686 \
        mesa-libGL.i686 mesa-dri-drivers.i686 libXcursor.i686 libXi.i686 libXrandr.i686 \
        libXScrnSaver.i686 libXfixes.i686 systemd-libs.i686 dbus-libs.i686

Arch, Manjaro, EndeavourOS: the 32-bit packages are in the multilib repository. Enable it
once (uncomment the [multilib] section of /etc/pacman.conf; this command does that), then
install:

    sudo sed -i '/^#\[multilib\]/,/^#Include/ s/^#//' /etc/pacman.conf
    sudo pacman -Syu --needed lib32-glibc lib32-libx11 lib32-libxext lib32-libpulse \
        lib32-alsa-lib lib32-mesa lib32-libxcursor lib32-libxi lib32-libxrandr lib32-libxss \
        lib32-libxfixes lib32-systemd lib32-dbus

With NVIDIA's own driver, OpenGL also needs its 32-bit part (Debian: the i386 package of the
driver's GL libraries; Ubuntu: libnvidia-gl-<version>:i386; Fedora: RPM Fusion's
xorg-x11-drv-nvidia-libs.i686; Arch: lib32-nvidia-utils). Without it the game scales in
software.

Controls
--------

    GBA         Keyboard        Gamepad (Xbox layout)
    D-pad       arrow keys      d-pad or left stick
    A           Z               A
    B           X               B
    L           A               LB or left trigger
    R           S               RB or right trigger
    Start       Enter           Start / Menu
    Select      Backspace       Back / View

    F11 or Alt+Enter   fullscreen on/off
    Escape             leave fullscreen
    close the window   quit (the game is saved first)

Gamepads that SDL knows (Xbox, PlayStation, Switch Pro and most others) work when plugged in,
also while the game runs (if one is not seen: your user needs read access to /dev/input/event*,
which desktop distributions give the logged-in user).

Settings: kururin.ini
---------------------

The first time the game runs it writes kururin.ini next to kururin_pc, with comments. Open it
with a text editor to change:

    [video]     scale = 3            the window is 240x160 times this (1-16)
                fullscreen = no      start in fullscreen
    [sound]     volume = 100         0-100
                mute = no
    [game]      pause_on_focus_loss = yes
                                     pause while another window has the focus
    [keyboard]  A = Z ...            each GBA button: one or more key names, separated by
                                     commas (e.g. "A = Z, Space"); names as in
                                     https://wiki.libsdl.org/SDL2/SDL_Keycode
    [gamepad]   A = a ...            each GBA button: gamepad buttons (a b x y back start
                                     leftshoulder rightshoulder dpup dpdown dpleft dpright ...)
                                     or stick and trigger directions (leftx- leftx+ lefty-
                                     lefty+ rightx- ... lefttrigger righttrigger)
                deadzone = 50        how far a stick must move to count (percent)

Delete kururin.ini to get the defaults back.

Saving
------

The game saves the way the cartridge does (after a level, in the menus), into

    kururin.sav

next to kururin_pc, as soon as the game has written its save and again when you quit. It is
the cartridge's 32 KiB SRAM: the same format as an emulator's .sav for this game, so you can
copy one in (named kururin.sav) or take yours to an emulator. Keep the folder somewhere you can
write to.

Command line (optional)
-----------------------

    ./kururin.sh --check             only check the libraries
    ./kururin.sh --rom PATH          another ROM file
                 --save PATH         another save file; --no-save: none
                 --config PATH       another settings file
                 --scale N  --fullscreen  --volume N  --mute
                                     (these win over kururin.ini)

kururin.sh passes these on to kururin_pc, which can also be started directly. Environment:
KURURIN_NO_CHECK=1 skips the library check; SDL's own variables work, e.g.
SDL_VIDEODRIVER=wayland (Wayland without XWayland: needs the 32-bit libwayland-client,
-cursor, -egl, libxkbcommon and libEGL) or SDL_AUDIODRIVER=alsa.

If something goes wrong
-----------------------

- "cannot open ... baserom.gba": the ROM is not next to kururin_pc, or not named baserom.gba
  (lower case).
- "... is not the expected ROM": a different version or a bad dump; only the European
  release (AKRP) works.
- "Permission denied" for ./kururin.sh: the archive was unpacked without its file modes (some
  graphical archive tools); unpack it with tar as above, or: chmod +x kururin.sh kururin_pc
- "No such file or directory" for ./kururin_pc although it is there: the 32-bit glibc is
  missing; ./kururin.sh says how to install it.
- Any error message is also written to kururin.log next to kururin_pc: send it along with a
  report, with the output of ./kururin.sh --check.
- No sound: install libpulse (above); with ALSA alone on a PipeWire or PulseAudio desktop,
  ALSA's 32-bit plugin for it is needed too (Debian/Ubuntu: libasound2-plugins:i386 or
  pipewire-alsa's 32-bit package). The game runs without sound.

Licences
--------

kururin_pc is built from the source of the kururin project (https://github.com/an1n-dya/kururin).
It contains GCC's runtime library (libgcc), under the GPL with the GCC Runtime Library Exception,
and uses the system's glibc (LGPL). libSDL2-2.0.so.0 is SDL 2.32.10 (https://www.libsdl.org/),
built from its source release, under the zlib licence: LICENSE-SDL2.txt and README-SDL.txt.
The renderer is derived from mGBA (https://mgba.io, Copyright (c) 2013-2015 Jeffrey Pfau) and is
under the Mozilla Public License 2.0: LICENSE-MPL-2.0.txt; its source is source/video.c.
Kuru Kuru Kururin is (c) Nintendo / Eighting: the program holds the game's code,
decompiled and compiled again, and none of the cartridge's data.
