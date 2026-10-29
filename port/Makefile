# The PC port (docs/port.md): builds port/kururin_pc, from either directory:
#
#   make -C port              (or, from the repository root: make -f port/Makefile)
#   make -C port windows      port/kururin_pc.exe and SDL2.dll: 32-bit Windows, with mingw-w64
#   make -C port dist-linux   dist/kururin_pc_linux_x86.tar.gz: the binary (stripped, RUNPATH $ORIGIN),
#                             libSDL2 built here with its backends loaded at run time, kururin.sh, README.txt
#                             (port/tools/package_linux.py; checked for ROM content and the glibc it needs).
#                             The release is built on Ubuntu 22.04: port/tools/build_linux_release.sh
#   make -C port dist-windows dist/kururin_pc_win32.zip: the exe, SDL2.dll, README.txt, licences
#                             (port/tools/package_windows.py; checked to hold no ROM content)
#   make -C port test-wine / test-audio-wine / test-video-wine / test-config-wine   the same tests with the Windows
#                             build under Wine (WINE_ARGS="--all" for every input; OCCUPIED=1: with the
#                             GBA's ranges taken before the game runs, port/test/occupy.c)
#   make -C port test         the port test: RAM against the original, frame by frame (port/test)
#   make -C port test-audio   the audio test: m4a's RAM and PCM buffer against the original, every frame
#   make -C port test-video   the pixel test: the screen against the original's, every frame (port/test/video_test.py)
#   make -C port test-config  kururin.ini and the gamepad mapping (port/test/config_test.c, SDL's virtual joystick)
#   make -C port survey       the display features the original uses, on every input (port/test/survey.c)
#   make -C port sound-survey the sound hardware and m4a features the original uses, on every input
#   make -C port layouts      struct layouts, agbcc vs the port's compiler (port/tools/check_layouts.py)
#   make -C port layouts-windows   the same with the Windows build's compiler
#   make -C port clean
#
# Needs gcc-multilib and libsdl2-dev:i386 (docs/port.md, Building; Fedora: glibc-devel.i686,
# libatomic.i686 and sdl2-compat-devel.i686); the Windows build needs
# mingw-w64 (gcc-mingw-w64-i686) and downloads SDL2's MinGW package (port/tools/get_sdl2_mingw.sh). The game's C is src/*.c,
# compiled with -DPLATFORM_PC. This file is the port's whole build: the top Makefile (locked)
# and the generated modules.mk are only read, never changed.

# Every path is absolute, from where this file is, so `make -C port` and
# `make -f port/Makefile` behave the same. Make splits paths at spaces, so a checkout whose path
# has one cannot build the port (the GBA build and make test can).
ifneq ($(word 2,$(CURDIR)),)
$(error the port cannot be built in a directory whose path has a space ($(CURDIR)): move the checkout)
endif
PORT     := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
ROOT     := $(patsubst %/,%,$(dir $(PORT)))
BUILD    := $(PORT)/build
TARGET   := $(PORT)/kururin_pc
PYTHON   := $(shell [ -x $(ROOT)/.venv/bin/python3 ] && echo $(ROOT)/.venv/bin/python3 || echo python3)

CC       := gcc
ARCH     := -m32
# The 32-bit SDL2's pkg-config file, where Debian/Ubuntu, Arch and Fedora put it (PKG_CONFIG_LIBDIR:
# not the 64-bit one in the default path)
PKG      := PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/lib32/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig pkg-config
SDL_CFLAGS := $(shell $(PKG) --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell $(PKG) --libs sdl2 2>/dev/null)
# The recipes that build with it check it first (so `make windows` or `clean` do not need it)
NEED_SDL  = $(if $(SDL_LIBS),,$(error no 32-bit SDL2 found: install libsdl2-dev:i386 (Debian/Ubuntu, after \
	dpkg --add-architecture i386) or sdl2-compat-devel.i686 (Fedora, with glibc-devel.i686 and libatomic.i686); \
	Arch's multilib has no 32-bit SDL2))

# The ARM semantics the game's C was written for (docs/port.md, "A 32-bit build"): unsigned
# char, wrapping signed arithmetic, no type-based aliasing; and no merging of identical
# functions (function addresses are compared and mapped back to GBA addresses).
SEMANTICS := -funsigned-char -fwrapv -fno-strict-aliasing -fno-ipa-icf -fno-common
OPT       := -O2 -g
# The game's C is pre-C23: from GCC 15 the default standard is C23, where src/m4a.c's `void (*)()`
# function pointers take no arguments and its calls through them do not compile.
GAME_STD  := -std=gnu17
# -w: the game's C is checked by its own build (agbcc, warning-free); GCC's -Wall adds only
# "maybe uninitialized" for variables a state machine sets in an earlier state, unsigned/signed
# name pointers and Heap_Init's unused return value (checked 2026-10-29: none is reached).
GAME_CFLAGS := $(ARCH) $(OPT) $(SEMANTICS) $(GAME_STD) -DPLATFORM_PC -iquote $(ROOT)/include -w
PORT_CFLAGS := $(ARCH) $(OPT) $(SEMANTICS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(SDL_CFLAGS)
# Linked at 0x10000000: the i386 default (0x08048000) is inside the GBA's ROM addresses.
LDFLAGS   := $(ARCH) -no-pie -Wl,-Ttext-segment=0x10000000

# The C modules of the GBA build, read from kururin.toml ([[modules]] with kind = "c": the list
# scripts/generate_asm.py writes into modules.mk as C_MODULES), except the SRAM library
# (port/src/agb_sram.c replaces it).
C_MODULES := $(shell $(PYTHON) -c "import tomllib; print(' '.join(m['name'] for m in \
	tomllib.load(open('$(ROOT)/kururin.toml', 'rb'))['modules'] if m['kind'] == 'c'))")
ifeq ($(C_MODULES),)
$(error cannot read the C modules from $(ROOT)/kururin.toml with $(PYTHON) (Python 3.11 or newer: ./setup.sh makes .venv))
endif
GAME_SRCS := $(patsubst %,$(ROOT)/src/%.c,$(filter-out agb_sram,$(C_MODULES)))
GAME_OBJS := $(patsubst $(ROOT)/src/%.c,$(BUILD)/game/%.o,$(GAME_SRCS))
PORT_SRCS := $(wildcard $(PORT)/src/*.c)
PORT_OBJS := $(patsubst $(PORT)/src/%.c,$(BUILD)/port/%.o,$(PORT_SRCS))
HEADERS   := $(wildcard $(ROOT)/include/*.h $(ROOT)/include/*/*.h)
# Every object depends on this file too: its flags change what they are
MAKEFILE  := $(PORT)/Makefile

.PHONY: all windows dist-linux dist-windows test test-audio test-video test-config test-wine test-audio-wine test-video-wine test-config-wine survey sound-survey layouts layouts-windows clean
all: $(TARGET)

$(BUILD)/game/%.o: $(ROOT)/src/%.c $(HEADERS) $(MAKEFILE)
	@mkdir -p $(@D)
	@echo "cc $(patsubst $(ROOT)/%,%,$<)"
	@$(CC) $(GAME_CFLAGS) -c $< -o $@

$(BUILD)/port/%.o: $(PORT)/src/%.c $(wildcard $(PORT)/src/*.h) $(MAKEFILE)
	@$(NEED_SDL)mkdir -p $(@D)
	@echo "cc $(patsubst $(ROOT)/%,%,$<)"
	@$(CC) $(PORT_CFLAGS) -c $< -o $@

# Addresses only (no ROM content): see port/tools/gen_port_tables.py. One run writes both files
# (&: a grouped target, GNU make 4.3: with two plain targets make -j ran it twice at once).
$(BUILD)/port_tables.c $(BUILD)/gba_symbols.ld &: $(PORT)/tools/gen_port_tables.py $(GAME_OBJS) $(PORT_OBJS) \
		$(ROOT)/kururin.toml $(ROOT)/config/functions.txt $(ROOT)/config/data_pointers.txt
	@echo "gen_port_tables.py"
	@$(PYTHON) $(PORT)/tools/gen_port_tables.py $(BUILD) $(GAME_OBJS) $(PORT_OBJS)

$(BUILD)/port_tables.o: $(BUILD)/port_tables.c $(PORT)/src/port.h $(MAKEFILE)
	@$(CC) $(PORT_CFLAGS) -Wno-unused -iquote $(PORT)/src -c $< -o $@

$(TARGET): $(GAME_OBJS) $(PORT_OBJS) $(BUILD)/port_tables.o $(BUILD)/gba_symbols.ld
	@echo "ld port/kururin_pc"
	@$(CC) $(LDFLAGS) $(GAME_OBJS) $(PORT_OBJS) $(BUILD)/port_tables.o $(BUILD)/gba_symbols.ld $(SDL_LIBS) -lm -o $@

test: $(TARGET)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py

AUDIO_INPUTS ?= boot_attract save_ending save_makeup
test-audio: $(TARGET)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py --extra-watch port/test/audio_watch.txt $(AUDIO_INPUTS)

test-video: $(TARGET)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/video_test.py $(VIDEO_ARGS)

$(BUILD)/config_test: $(PORT)/test/config_test.c $(PORT)/src/config.c $(PORT)/src/port.h $(MAKEFILE)
	@$(NEED_SDL)mkdir -p $(@D)
	@$(CC) $(PORT_CFLAGS) $(PORT)/test/config_test.c $(PORT)/src/config.c $(SDL_LIBS) -o $@

test-config: $(BUILD)/config_test
	@$(BUILD)/config_test $(BUILD)

survey:
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py --all --survey

sound-survey:
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py --all --sound-survey

layouts:
	@cd $(ROOT) && $(PYTHON) port/tools/check_layouts.py

layouts-windows:
	@cd $(ROOT) && $(PYTHON) port/tools/check_layouts.py --cc "$(WIN_CC) -mno-ms-bitfields"

# --- The Linux package (docs/port.md, "The Linux package") ---------------------------------------

# SDL2 built for the package (port/tools/build_sdl2_linux.sh): needs only libc and libm, loads X11,
# Wayland, ALSA, PulseAudio... at run time if the system has them
SDL2_LINUX := $(BUILD)/sdl2-linux/prefix
LINUX_DIST := $(BUILD)/linux-dist
DIST_PORT_OBJS := $(patsubst $(PORT)/src/%.c,$(LINUX_DIST)/port/%.o,$(PORT_SRCS))

$(SDL2_LINUX)/lib/libSDL2-2.0.so.0:
	@sh $(PORT)/tools/build_sdl2_linux.sh $(BUILD)/sdl2-linux

$(LINUX_DIST)/port/%.o: $(PORT)/src/%.c $(wildcard $(PORT)/src/*.h) $(SDL2_LINUX)/lib/libSDL2-2.0.so.0 $(MAKEFILE)
	@mkdir -p $(@D)
	@echo "dist cc $(patsubst $(ROOT)/%,%,$<)"
	@$(CC) $(ARCH) $(OPT) $(SEMANTICS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter -I$(SDL2_LINUX)/include/SDL2 \
		-D_REENTRANT -c $< -o $@

# The game's objects are the build's own (the same flags); the port's are built against the
# bundled SDL. RUNPATH $ORIGIN: libSDL2-2.0.so.0 is found next to the binary. Stripped.
$(LINUX_DIST)/kururin_pc: $(GAME_OBJS) $(DIST_PORT_OBJS) $(BUILD)/port_tables.o $(BUILD)/gba_symbols.ld
	@echo "ld (dist) kururin_pc"
	@$(CC) $(LDFLAGS) -static-libgcc $(GAME_OBJS) $(DIST_PORT_OBJS) $(BUILD)/port_tables.o $(BUILD)/gba_symbols.ld \
		-L$(SDL2_LINUX)/lib -lSDL2 -lm -Wl,-rpath,'$$ORIGIN' -o $@
	@strip --strip-all $@

dist-linux: $(LINUX_DIST)/kururin_pc
	@$(PYTHON) $(PORT)/tools/package_linux.py $(ROOT)/dist/kururin_pc_linux_x86.tar.gz $(LINUX_DIST)/kururin_pc \
		$(BUILD)/sdl2-linux

# --- Windows: the same sources with the 32-bit mingw-w64 compiler (docs/port.md, "Windows") ---

WIN_CC     := i686-w64-mingw32-gcc
WIN_NM     := i686-w64-mingw32-nm
WIN_BUILD  := $(BUILD)/win
WIN_TARGET := $(PORT)/kururin_pc.exe
SDL2_DIR   := $(BUILD)/sdl2-mingw/SDL2-2.32.10/i686-w64-mingw32
# -mno-ms-bitfields: GCC's bitfield layout (as on Linux and agbcc), not MSVC's, which mingw uses
# by default and which would change the layout of the game's bitfield structs
WIN_SEMANTICS   := $(SEMANTICS) -mno-ms-bitfields
WIN_GAME_CFLAGS := $(OPT) $(WIN_SEMANTICS) $(GAME_STD) -DPLATFORM_PC -iquote $(ROOT)/include -w
WIN_PORT_CFLAGS := $(OPT) $(WIN_SEMANTICS) -std=gnu11 -Wall -Wextra -Wno-unused-parameter \
                   -D__USE_MINGW_ANSI_STDIO=1 -I$(SDL2_DIR)/include/SDL2
# The GBA's memory is part of the exe's image (port/tools/win_image.py: sections at the GBA
# addresses, the exe's code from 0x10000000), based at 0x00400000 with no relocations and no ASLR
# (the absolute GBA symbols are not relocatable either); a GUI program (no console window);
# large-address-aware: on 64-bit Windows the DLLs and heap get the 2-4 GiB range too.
WIN_LDFLAGS := -mwindows -static-libgcc -Wl,--disable-dynamicbase -Wl,--disable-reloc-section \
               -Wl,--large-address-aware
WIN_IMAGE_LDFLAGS := -Wl,--image-base,0x00400000 -Wl,-T,$(WIN_BUILD)/gba_image.ld
WIN_GAME_OBJS := $(patsubst $(ROOT)/src/%.c,$(WIN_BUILD)/game/%.o,$(GAME_SRCS))
WIN_PORT_OBJS := $(patsubst $(PORT)/src/%.c,$(WIN_BUILD)/port/%.o,$(PORT_SRCS))

windows: $(WIN_TARGET) $(PORT)/SDL2.dll

$(SDL2_DIR)/lib/libSDL2.dll.a:
	@sh $(PORT)/tools/get_sdl2_mingw.sh $(BUILD)/sdl2-mingw

$(WIN_BUILD)/game/%.o: $(ROOT)/src/%.c $(HEADERS) $(MAKEFILE)
	@mkdir -p $(@D)
	@echo "win cc $(patsubst $(ROOT)/%,%,$<)"
	@$(WIN_CC) $(WIN_GAME_CFLAGS) -c $< -o $@

$(WIN_BUILD)/port/%.o: $(PORT)/src/%.c $(wildcard $(PORT)/src/*.h) $(SDL2_DIR)/lib/libSDL2.dll.a $(MAKEFILE)
	@mkdir -p $(@D)
	@echo "win cc $(patsubst $(ROOT)/%,%,$<)"
	@$(WIN_CC) $(WIN_PORT_CFLAGS) -c $< -o $@

$(WIN_BUILD)/port_tables.c $(WIN_BUILD)/gba_symbols.ld &: $(PORT)/tools/gen_port_tables.py $(WIN_GAME_OBJS) \
		$(WIN_PORT_OBJS) $(ROOT)/kururin.toml $(ROOT)/config/functions.txt $(ROOT)/config/data_pointers.txt
	@echo "gen_port_tables.py (windows)"
	@$(PYTHON) $(PORT)/tools/gen_port_tables.py --nm $(WIN_NM) --prefix _ $(WIN_BUILD) $(WIN_GAME_OBJS) $(WIN_PORT_OBJS)

$(WIN_BUILD)/port_tables.o: $(WIN_BUILD)/port_tables.c $(PORT)/src/port.h $(MAKEFILE)
	@$(WIN_CC) $(WIN_PORT_CFLAGS) -Wno-unused -iquote $(PORT)/src -c $< -o $@

$(WIN_BUILD)/gba_image.S: $(PORT)/tools/win_image.py
	@mkdir -p $(@D)
	@$(PYTHON) $(PORT)/tools/win_image.py asm $@

$(WIN_BUILD)/gba_image.o: $(WIN_BUILD)/gba_image.S
	@$(WIN_CC) -c $< -o $@

$(WIN_BUILD)/gba_image.ld: $(PORT)/tools/win_image.py
	@mkdir -p $(@D)
	@$(PYTHON) $(PORT)/tools/win_image.py ld i686-w64-mingw32-ld $@

$(WIN_TARGET): $(WIN_GAME_OBJS) $(WIN_PORT_OBJS) $(WIN_BUILD)/port_tables.o $(WIN_BUILD)/gba_symbols.ld \
		$(WIN_BUILD)/gba_image.o $(WIN_BUILD)/gba_image.ld
	@echo "ld port/kururin_pc.exe"
	@$(WIN_CC) $(WIN_LDFLAGS) $(WIN_IMAGE_LDFLAGS) $(WIN_GAME_OBJS) $(WIN_PORT_OBJS) $(WIN_BUILD)/port_tables.o \
		$(WIN_BUILD)/gba_image.o $(WIN_BUILD)/gba_symbols.ld -L$(SDL2_DIR)/lib -lSDL2 -lm -o $@.tmp
	@$(PYTHON) $(PORT)/tools/win_image.py patch $@.tmp
	@mv $@.tmp $@

# For the tests only: the same exe without the GBA sections (its memory from VirtualAlloc, as
# before), to check the fallback and the diagnostics when a range is taken
$(WIN_BUILD)/kururin_pc_noimage.exe: $(WIN_GAME_OBJS) $(WIN_PORT_OBJS) $(WIN_BUILD)/port_tables.o \
		$(WIN_BUILD)/gba_symbols.ld
	@$(WIN_CC) $(WIN_LDFLAGS) -Wl,--image-base,0x10000000 $(WIN_GAME_OBJS) $(WIN_PORT_OBJS) \
		$(WIN_BUILD)/port_tables.o $(WIN_BUILD)/gba_symbols.ld -L$(SDL2_DIR)/lib -lSDL2 -lm -o $@
	@cp $(SDL2_DIR)/bin/SDL2.dll $(WIN_BUILD)/

dist-windows: windows
	@$(PYTHON) $(PORT)/tools/package_windows.py $(ROOT)/dist/kururin_pc_win32.zip $(WIN_TARGET) $(SDL2_DIR)

$(PORT)/SDL2.dll: $(SDL2_DIR)/lib/libSDL2.dll.a
	@cp $(SDL2_DIR)/bin/SDL2.dll $@

# The Windows build under Wine (docs/port.md, "Testing the Windows build"); the prefix is Wine's
# default (~/.wine) unless WINEPREFIX says otherwise
WINE ?= env WINEDEBUG=-all WINEDLLOVERRIDES=mscoree,mshtml= wine
# OCCUPIED=1: through port/test/occupy.c, the GBA's ranges (OCCUPY) taken before the game runs
OCCUPY ?= 0x02000000 0x03000000 0x04000000 0x05000000 0x06000000 0x07000000 0x08000000 0x0E000000
WINE_PORT := --port "$(WINE) $(if $(OCCUPIED),$(WIN_BUILD)/occupy.exe $(OCCUPY) -- )$(WIN_TARGET)"
WINE_DEPS := windows $(if $(OCCUPIED),$(WIN_BUILD)/occupy.exe)
$(WIN_BUILD)/occupy.exe: $(PORT)/test/occupy.c
	@mkdir -p $(@D)
	@$(WIN_CC) -O2 -Wall $< -o $@

test-wine: $(WINE_DEPS)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py $(WINE_PORT) $(WINE_ARGS)

test-audio-wine: $(WINE_DEPS)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/port_test.py $(WINE_PORT) --extra-watch port/test/audio_watch.txt \
		$(or $(WINE_ARGS),$(AUDIO_INPUTS))

$(WIN_BUILD)/config_test.exe: $(PORT)/test/config_test.c $(PORT)/src/config.c $(PORT)/src/port.h $(SDL2_DIR)/lib/libSDL2.dll.a
	@mkdir -p $(@D)
	@$(WIN_CC) $(WIN_PORT_CFLAGS) $(PORT)/test/config_test.c $(PORT)/src/config.c -L$(SDL2_DIR)/lib -lSDL2 -o $@
	@cp $(SDL2_DIR)/bin/SDL2.dll $(WIN_BUILD)/

test-config-wine: $(WIN_BUILD)/config_test.exe
	@$(WINE) $(WIN_BUILD)/config_test.exe $(WIN_BUILD)

test-video-wine: $(WINE_DEPS)
	@$(MAKE) -C $(PORT)/test
	@cd $(ROOT) && $(PYTHON) port/test/video_test.py $(WINE_PORT) $(WINE_ARGS)

clean:
	rm -rf $(BUILD) $(TARGET) $(WIN_TARGET) $(PORT)/SDL2.dll
	@$(MAKE) -C $(PORT)/test clean
