// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// Natural C for Sprite_DebugTileGrid. old_agbcc folds each single-use ldrh of an OAM halfword
// into its and (its thumb.c s_register_operand accepts a subreg of a memory operand) and
// reloads it next to the and; the ROM keeps the load first (ldrh r1; adds r0, r6, #0;
// ands r0, r1), as agbcc does. old_agbcc reaches 90.91% (only volatile bitfields close
// the gap). With Util_StepTowards.c this makes game agbcc -fprologue-bugfix.

#include "gba/types.h"

// gMain (include/main.h), up to the halfword at +0x2C this function tests
struct MainView
{
    u8 unk0[0x2C];
    u16 unk2C;
};
extern struct MainView gMain;

// One 0xC-byte entry of the block Sprite_AllocMany hands out: OAM attributes, then 6 bytes
struct SpriteEntry
{
    u8 y;
    u8 attr0hi;
    u16 x:9;
    u16 attr1hi:7;
    u16 attr2;
    u16 unk6;
    u32 unk8;
};

struct SpriteEntry *Sprite_AllocMany(u8 layer, u8 count);

void Sprite_DebugTileGrid(void)
{
    s32 a = (gMain.unk2C & 0x10) ? 0x10 : 0;
    s32 b = (gMain.unk2C & 0x80) ? 0x200 : 0;
    s32 i;

    for (i = 0; i < 16; i++)
    {
        struct SpriteEntry *s = Sprite_AllocMany(0, 4);
        s32 tile;

        if (s == NULL)
            return;
        *(u32 *)&s[0] = *(u32 *)&s[1] = *(u32 *)&s[2] = *(u32 *)&s[3] = 0x40004000;
        tile = b + a + i * 32;
        s[0].attr2 = tile;
        s[1].attr2 = tile + 4;
        s[2].attr2 = tile + 8;
        s[3].attr2 = tile + 12;
        s[0].x = 0;
        s[1].x = 0x20;
        s[2].x = 0x40;
        s[3].x = 0x60;
        s[0].y = s[1].y = s[2].y = s[3].y = i * 8;
    }
}
