// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
#ifndef GUARD_TEXT_H
#define GUARD_TEXT_H

// src/text.c (08002848-08003C18): times, numbers and text drawn as sprites, text conversion. The
// functions other modules call.

#include "game.h"

void Text_DrawSmall(s32 x, u8 y, const u8 *str, u8 palette);
void Text_DrawSmallHex(u16 x, u8 y, u32 value, u8 digits, u8 palette);
void Text_DrawSmallNumber(u16 x, u8 y, s16 value, u8 palette);
void Text_DrawSmallTime(u16 x, u8 y, u32 frames, u8 palette);
void Text_DrawTime(u16 x, u8 y, u32 frames, u8 palette);
void Text_Draw(s32 x, u8 y, const u8 *str, u8 palette);
void Text_DrawNumber(u16 x, u8 y, s16 value, u8 palette);
void Text_DrawNumberAligned(s32 x, s32 y, u32 value, s32 digits, u8 palette);
void Text_DrawSmallNumberAligned(s32 x, s32 y, u32 value, s32 digits, u8 palette);
void Text_PrintSmall(struct TextCursor *p, const u8 *str);
void Text_Print(struct TextCursor *p, const u8 *str);
void Text_DrawDigitTime(u8 a, u16 x, u8 y, u32 frames, u8 palette);
s32 Text_ConvertToU16(const u8 *str, u16 *dst);
s32 Text_Convert(const u8 *src, u8 *dst);
void Text_DrawSmallName(s32 x, u8 y, const u8 *str, u8 suffix, u8 palette);
void Text_DrawName(s32 x, u8 y, const u8 *str, u8 a, u8 palette);
void Text_PrintSmallProportional(struct TextCursor *p, const u8 *str);
void Text_PrintProportional(struct TextCursor *p, const u8 *str);

#endif // GUARD_TEXT_H
