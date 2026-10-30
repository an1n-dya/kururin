Kuru Kuru Kururin - PC port for Windows
=======================================

A 32-bit program for 32- and 64-bit Windows (7 or later). Tested on Windows and under
Wine 9.0.

This is the game's own code, decompiled to C and compiled for the PC, with a small layer
standing in for the Game Boy Advance's hardware. It contains none of the cartridge's data:
no graphics, no music, no levels. Those come from your own copy of the cartridge.

Getting started
---------------

1. Make a dump of your own Kuru Kuru Kururin cartridge (Europe, AKRP) and name it

       baserom.gba

   and put it in this folder, next to kururin_pc.exe. It must be exactly that ROM:
   4 194 304 bytes, SHA-1 6025ef597db2684ae064ffe22b4fd1d37941f887. The game checks it at
   start-up and says so if the file is missing or different.

2. Double-click kururin_pc.exe.

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
also while the game runs.

Settings: kururin.ini
---------------------

The first time the game runs it writes kururin.ini next to the exe, with comments. Open it with
Notepad to change:

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

next to the exe, as soon as the game has written its save and again when you quit. It is the
cartridge's 32 KiB SRAM: the same format as an emulator's .sav for this game, so you can copy
one in (named kururin.sav) or take yours to an emulator. Keep the folder somewhere you can write
to (not "Program Files").

Command line (optional)
-----------------------

    kururin_pc.exe --rom PATH        another ROM file
                   --save PATH       another save file; --no-save: none
                   --config PATH     another settings file
                   --scale N  --fullscreen  --volume N  --mute
                                     (these win over kururin.ini)

Run from a command prompt, the program prints its messages there.

If something goes wrong
-----------------------

- "cannot open ... baserom.gba": the ROM is not next to the exe, or not named baserom.gba.
- "... is not the expected ROM": a different version or a bad dump; only the European
  release (AKRP) works.
- Any error message is also written to kururin.log next to the exe: send it along with a report.
- "cannot reserve the GBA's ... at 0x...": the program keeps the GBA's memory at the GBA's own
  addresses, as part of the exe itself, so this should not happen; the message (and
  kururin.log) lists what holds that memory instead. Please report it with the log.
- "The application was unable to start correctly": Windows could not place the exe at its
  fixed address (0x00400000), which it needs. One known cause: Windows Security > App & browser
  control > Exploit protection, "Force randomization for images (Mandatory ASLR)" with "Do not
  allow stripped images"; add a program setting for kururin_pc.exe that turns it off.
- No sound: the game runs without; check Windows' sound output device.
- Windows SmartScreen may warn about an unknown program; "More info" > "Run anyway".

Licences
--------

kururin_pc.exe is built from the source of the kururin project (https://github.com/an1n-dya/kururin).
SDL2.dll is SDL 2.32.10 (https://www.libsdl.org/), under the zlib licence: LICENSE-SDL2.txt and README-SDL.txt. The
program contains the mingw-w64 runtime, under the ZPL 2.1 and the other licences listed in
LICENSE-mingw-w64.txt, and GCC's runtime library (libgcc), under the GPL with the GCC Runtime
Library Exception. The renderer is derived from mGBA (https://mgba.io, Copyright (c) 2013-2015
Jeffrey Pfau) and is under the Mozilla Public License 2.0: LICENSE-MPL-2.0.txt; its source is
source/video.c. Kuru Kuru Kururin is (c) Nintendo / Eighting: the program holds the game's
code, decompiled and compiled again, and none of the cartridge's data.
