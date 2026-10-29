// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_DECOMPRESS_H
#define GUARD_DECOMPRESS_H

// src/decompress.c (08004030-080040E0): the run-length and LZ decompressors. The functions other
// modules call.

#include "game.h"

s32 Decompress_LZ(const u8 *src, u8 *dst, s32 size);

#endif // GUARD_DECOMPRESS_H
