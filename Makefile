# Kuru Kuru Kururin (Europe) -- functional decompilation.
#
#   make test             clean build + gameplay equivalence with baserom.gba (the verdict);
#                         a full run also writes docs/coverage.md
#   make shift-test       make test with 0x100 bytes of padding after crt0 (everything moves)
#   make compare          clean build + SHA1 check against baserom.gba (informational)
#   make pointers         rerun scripts/find_pointers.py (config/data_pointers.txt) on fresh
#                         evidence (make pointer-evidence)
#   make rom              incremental build, no check (fast iteration)
#   make expected         objdiff target objects (expected/src/<module>.o)
#   make ghidra-functions Ghidra headless function export for discovery
#   make tidy             remove build outputs
#
# Generated inputs (asm/, data/, ldscript.txt, modules.mk) come from
# `python3 scripts/generate_asm.py` (run by ./setup.sh). Nothing generated is committed.

BUILD_NAME := kururin
ROM        := $(BUILD_NAME).gba
ELF        := $(BUILD_NAME).elf
MAP        := $(BUILD_NAME).map
BUILD_DIR  := build

SHELL  := /bin/bash -o pipefail
MAKEFLAGS += --no-print-directory
.SUFFIXES:
.SECONDARY:
.DELETE_ON_ERROR:
.SECONDEXPANSION:

PREFIX    := arm-none-eabi-
AS        := $(PREFIX)as
LD        := $(PREFIX)ld
AR        := $(PREFIX)ar
OBJCOPY   := $(PREFIX)objcopy
CPP       := $(PREFIX)cpp
AGBCC     := tools/agbcc/bin/agbcc
OLD_AGBCC := tools/agbcc/bin/old_agbcc
PYTHON    := $(shell [ -x .venv/bin/python3 ] && echo .venv/bin/python3 || echo python3)

ASFLAGS  := -mcpu=arm7tdmi -mthumb-interwork
# -std=gnu17: the system's arm-none-eabi-cpp preprocesses for agbcc, and from GCC 15 its default
# is C23, where src/m4a.c's `#if __STDC_VERSION__ < 202311L` takes the branch agbcc cannot parse.
CPPFLAGS := -std=gnu17 -nostdinc -I tools/agbcc/include -iquote include
# No default: every C module's cc/cflags come from kururin.toml, through modules.mk.
CC1       = $(error $@: no cc in modules.mk (kururin.toml cc/cflags))
CC1FLAGS  = $(error $@: no cflags in modules.mk (kururin.toml cc/cflags))

# Gameplay equivalence harness (tests/gameplay), linked against the libmgba setup.sh builds.
HOSTCC   ?= cc
MGBA     := tools/mgba
KURUTEST := $(BUILD_DIR)/test/kurutest

GHIDRA_INSTALL_DIR ?= $(abspath tools/ghidra)
GHIDRA_OUT := ghidra_out

ifeq ($(filter all compare test shift-test pointers pointer-evidence check-compilers tidy clean ghidra-functions,$(or $(MAKECMDGOALS),all)),)
ifeq ($(wildcard modules.mk),)
$(error modules.mk is missing: run ./setup.sh (or $(PYTHON) scripts/generate_asm.py) first)
endif
include modules.mk
endif

.PHONY: all compare test shift-test pointers pointer-evidence check-compilers rom tidy clean expected ghidra-functions

all: test

# Always from scratch, and from freshly generated asm: an incremental build can skip
# relinking and then check the PREVIOUS ROM, and asm left over from an older
# kururin.toml / functions.txt can pass here yet fail in a fresh checkout.
# Use `make rom` for fast iteration. It also checks every module's cc/cflags against
# tests/compiler_evidence (scripts/check_compilers.py), before the SHA1: the SHA1 check fails
# by design now that the game's C is not byte-identical, and would stop the recipe first.
compare:
	@ALLOW_NO_GHIDRA= $(PYTHON) scripts/generate_asm.py
	@$(MAKE) tidy
	@$(MAKE) rom
	@$(PYTHON) scripts/check_compilers.py --generated
	@sha1sum -c $(BUILD_NAME).sha1

# The verdict: from freshly generated asm and a clean build (like compare), run baserom.gba and
# the rebuilt ROM side by side in mGBA on every input in tests/gameplay/inputs/, comparing the
# watched RAM (tests/gameplay/watch.txt) every frame and the screen every 4 frames.
# TEST_ARGS goes to tests/gameplay/run.py (e.g. TEST_ARGS="--quick" or an input's name).
# ROM addresses the game keeps in RAM are compared through build/test/rom_map.txt
# (scripts/rom_map.py: where each original address is in the rebuilt ROM). The original runs
# traced, recording what it executes; a full run turns that into docs/coverage.md.
# Both tests first check the test lock (tests/check_lock.py: the harness, inputs, watch list,
# address map and this Makefile must hash as in tests/lock.sha256).
test:
	@$(PYTHON) tests/check_lock.py
	@ALLOW_NO_GHIDRA= $(PYTHON) scripts/generate_asm.py
	@$(MAKE) tidy
	@$(MAKE) rom
	@$(MAKE) $(KURUTEST)
	@$(PYTHON) scripts/rom_map.py $(ELF) > $(BUILD_DIR)/test/rom_map.txt
	@$(PYTHON) tests/gameplay/run.py --kurutest $(KURUTEST) --rom-map $(BUILD_DIR)/test/rom_map.txt $(TEST_ARGS)

# Shiftability: the same test with SHIFT bytes of padding linked after crt0, so every function
# and all the data move. It passes only if every ROM address in code and data is a symbol
# (config/data_pointers.txt). The tree is regenerated without the padding afterwards.
SHIFT ?= 0x100
shift-test:
	@$(PYTHON) tests/check_lock.py
	@ALLOW_NO_GHIDRA= $(PYTHON) scripts/generate_asm.py --shift $(SHIFT)
	@$(MAKE) tidy
	@status=0; $(MAKE) rom && $(MAKE) $(KURUTEST) \
	  && $(PYTHON) scripts/rom_map.py $(ELF) > $(BUILD_DIR)/test/rom_map.txt \
	  && $(PYTHON) tests/gameplay/run.py --kurutest $(KURUTEST) --rom-map $(BUILD_DIR)/test/rom_map.txt \
	       --no-coverage $(TEST_ARGS) || status=$$?; \
	  ALLOW_NO_GHIDRA= $(PYTHON) scripts/generate_asm.py > /dev/null; \
	  if [ $$status = 0 ]; then echo "make shift-test: passed with $(SHIFT) bytes of padding"; fi; exit $$status

# Evidence for scripts/find_pointers.py: what the original loads from ROM on every input
# (build/test/*.loads). Only baserom.gba runs (it is compared with itself).
pointer-evidence:
	@$(MAKE) $(KURUTEST)
	@$(PYTHON) tests/gameplay/run.py --kurutest $(KURUTEST) --rom baserom.gba --no-coverage --rom-loads

pointers: pointer-evidence
	@$(PYTHON) scripts/find_pointers.py

$(KURUTEST): tests/gameplay/kurutest.c $(MGBA)/lib/libmgba.a
	@mkdir -p $(@D)
	@echo "cc $<"
	@$(HOSTCC) -O2 -Wall -Wextra -I$(MGBA)/include $< $(MGBA)/lib/libmgba.a -lm -lpthread -o $@

check-compilers:
	@$(PYTHON) scripts/check_compilers.py

rom: $(ROM)

$(ROM): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(ELF): $(OBJS) ldscript.txt
	@echo "$(LD) -T ldscript.txt -Map $(MAP) <objects> -o $@"
	@cd $(BUILD_DIR) && $(LD) -T ../ldscript.txt -Map ../$(MAP) $(patsubst $(BUILD_DIR)/%,%,$(OBJS)) -o ../$@

# C modules: INCLUDE_ASM'd functions are pasted in by agbcc as top-level asm.
$(BUILD_DIR)/src/%.o: src/%.c $$(wildcard asm/nonmatchings/%/*.s) $$(wildcard include/*.h include/*/*.h)
	@mkdir -p $(@D)
	@echo "$(notdir $(CC1)) $<"
	@$(CPP) $(CPPFLAGS) $< -o $(BUILD_DIR)/src/$*.i
	@$(CC1) $(CC1FLAGS) -o $(BUILD_DIR)/src/$*.s $(BUILD_DIR)/src/$*.i
	@printf "\t.text\n\t.align\t2, 0\n" >> $(BUILD_DIR)/src/$*.s
	@$(AS) $(ASFLAGS) -o $@ $(BUILD_DIR)/src/$*.s

$(BUILD_DIR)/asm/%.o: asm/%.s asm/macros.inc
	@mkdir -p $(@D)
	@echo "as $<"
	@$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/data/%.o: data/%.s baserom.gba
	@mkdir -p $(@D)
	@echo "as $<"
	@$(AS) $(ASFLAGS) -o $@ $<

# objdiff targets: each C module rebuilt purely from its generated per-function asm,
# in address order -- i.e. what the module's object must be once fully decompiled.
expected: $(patsubst %,expected/src/%.o,$(C_MODULES))

expected/src/%.o: $$(wildcard asm/nonmatchings/%/*.s) config/functions.txt
	@mkdir -p $(@D)
	@$(PYTHON) scripts/expected_asm.py $* > expected/src/$*.s
	@$(AS) $(ASFLAGS) -o $@ expected/src/$*.s

# Independent function list from Ghidra (ARM:LE:32:v4t raw import at 0x08000000),
# consumed by scripts/discover_functions.py.
ghidra-functions:
	@mkdir -p $(GHIDRA_OUT)/project
	@rm -f $(GHIDRA_OUT)/functions.txt
	$(GHIDRA_INSTALL_DIR)/support/analyzeHeadless $(GHIDRA_OUT)/project kururin \
		-import baserom.gba -overwrite -processor ARM:LE:32:v4t -cspec default \
		-loader BinaryLoader -loader-baseAddr 0x08000000 \
		-scriptPath scripts/ghidra -preScript GbaRomSetup.java \
		-postScript ExportFunctions.java $(GHIDRA_OUT)/functions.txt -deleteProject \
		> $(GHIDRA_OUT)/headless.log 2>&1
	@grep -q "Using Language/Compiler: ARM:LE:32:v4t" $(GHIDRA_OUT)/headless.log \
		|| { echo "Ghidra did not import as ARM:LE:32:v4t: see $(GHIDRA_OUT)/headless.log"; exit 1; }
	@tail -n 1 $(GHIDRA_OUT)/functions.txt 2>/dev/null | grep -E '^# end [0-9]+ functions$$' \
		|| { echo "Ghidra export failed: see $(GHIDRA_OUT)/headless.log"; exit 1; }

# expected/ survives tidy (asmlift's --score-against reads it); its rules rebuild it
# whenever the generated asm changes.
tidy:
	rm -rf $(BUILD_DIR)
	rm -f $(ROM) $(ELF) $(MAP)

clean: tidy
	rm -rf expected
