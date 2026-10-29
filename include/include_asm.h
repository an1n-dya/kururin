// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_INCLUDE_ASM_H
#define GUARD_INCLUDE_ASM_H

// INCLUDE_ASM(folder, name) pastes the generated asm/<folder>/<name>.s in place
// of a function that has not been decompiled yet. agbcc copies top-level asm
// verbatim into its output, so the function keeps its exact position and bytes.
// The generated files use unified syntax; agbcc's own output is divided.
//
// decomp-permuter preprocesses with -DPERMUTER and cannot parse top-level asm,
// so there INCLUDE_ASM only declares a dummy object.
#ifdef PERMUTER
#define INCLUDE_ASM(FOLDER, NAME) extern int permuter_include_asm_##NAME
#elif defined(PLATFORM_PC)
// The PC port (docs/port.md) compiles no asm: the functions still INCLUDE_ASM'd (the two
// [[asm_regions]]) are written in C under port/.
#define INCLUDE_ASM(FOLDER, NAME) extern int port_include_asm_##NAME
#else
// The asm is pushed into .text explicitly: after a data definition agbcc's current section
// is .data/.rodata, and the function would otherwise land there.
#define INCLUDE_ASM(FOLDER, NAME)                                                                   \
    __asm__(".pushsection .text\n"                                                                  \
            ".syntax unified\n"                                                                     \
            ".include \"" FOLDER "/" #NAME ".s\"\n"                                                 \
            ".syntax divided\n"                                                                     \
            ".popsection\n")

// Define the function macros once per translation unit.
__asm__(".include \"asm/macros.inc\"\n");
#endif

#endif // GUARD_INCLUDE_ASM_H
