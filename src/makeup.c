// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// makeup: lives by game mode and the Make Up screen.
// 080131F8-08013D0C, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "task.h"
#include "menu.h"
#include "save.h"
#include "intro.h"
#include "makeup.h"
#include "helirin.h"
#include "bonus.h"
#include "text.h"

extern const struct LivesSettings gDefaultLivesSettings;
extern const u16 gMakeUpObjPalettes[];
extern const struct TileCopy gMakeUpSpriteTiles[];
extern const void *const gMakeUpIconTiles[22];
extern const u8 gMakeUpIconPalettes[10];
extern u32 gMakeUpIconTile;
extern const u8 gMakeUpColorPalettes[7];
extern const u8 gMakeUpTypeColumns[];
extern const u8 gMakeUpColorColumns[];
s32 MakeUp_MenuItem(struct Menu *p, s32 item);
// Lives_SettingsMenuItem's lines
extern const u8 gUnk_0801ED08[];
extern const u8 gUnk_0801ED10[];
extern const u8 gUnk_0801ED1C[];
extern const u8 gUnk_0801ED28[];
extern const u8 gUnk_0801ED38[];
extern const u8 gUnk_0801ED40[];
extern const u8 *const gUnk_080C487C[2]; // "OFF", "ON"

/**
 * Resets gLivesSettings to its initial value, gDefaultLivesSettings.
 */
void Lives_Reset(void)
{
    gLivesSettings = gDefaultLivesSettings;
}
/**
 * Sets *h*'s lives (and maxLives, the same) and size for the game mode gGameState.mode:
 * 1 and 7: lives gLivesSettings.lives (0 in the demo); 2: the same, and size 5 (0 in the
 * demo); 3: challengeLives; 4 (four Helirins): linkLives[n] and size linkSizes[n] for Helirin n (h->player), with
 * type and color cleared and disableFlags bit 1 set; 5: mode 2 when gGameState.saveFile->flags has bit 4,
 * else mode 1; 6: debugLives. Then (not in mode 6, which only sets it) bit 4 of flags follows
 * bit 0 of gLivesSettings.invincible. Other modes change nothing.
 */
void Lives_Init(struct Helirin *h)
{
    switch ((s8)(gGameState.mode - 1))
    {
    case 5:
        h->size = 0;
        h->lives = h->maxLives = gLivesSettings.debugLives;
        h->flags |= 0x10;
        return;
    case 4:
        h->lives = 0;
        h->maxLives = 0;
        if (gGameState.saveFile->flags & 0x10)
            goto mode2;
        // fall through
    case 0:
    case 6:
        h->size = 0;
        if (gGameState.flags & GAME_FLAG_DEMO)
            h->lives = h->maxLives = 0;
        else
            h->lives = h->maxLives = gLivesSettings.lives;
        break;
    case 1:
    mode2:
        if (gGameState.flags & GAME_FLAG_DEMO)
        {
            h->size = 0;
            h->lives = h->maxLives = 0;
        }
        else
        {
            h->size = 5;
            h->lives = h->maxLives = gLivesSettings.lives;
        }
        break;
    case 2:
        h->size = 0;
        h->lives = h->maxLives = gLivesSettings.challengeLives;
        break;
    case 3:
        h->size = gLivesSettings.linkSizes[h->player];
        h->type = 0;
        h->color = 0;
        h->lives = gLivesSettings.linkLives[h->player];
        h->maxLives = gLivesSettings.linkLives[h->player];
        h->disableFlags |= 2;
        break;
    default:
        return;
    }
    if (gLivesSettings.invincible & 1)
        h->flags |= 0x10;
    else
        h->flags &= ~0x10;
}
/**
 * Starts one Bonus_UpdateWornItem task (0x34 bytes) for each set bit 0-9 of gGameState.saveFile->items,
 * with item = the bit number and point = the bit number + 1.
 */
void MakeUp_StartWornItems(void)
{
    u16 bits = gGameState.saveFile->items;
    struct WornItem *task;
    s32 i;

    for (i = 0; i < 10; i++)
    {
        if (bits & (1 << i))
        {
            task = (struct WornItem *)Task_Create(Bonus_UpdateWornItem, sizeof(*task));
            if (task != NULL)
            {
                task->item = i;
                task->point = i + 1;
            }
        }
    }
}
/**
 * A menu screen's loop: loads its graphics, starts one Bonus_UpdateMakeUpIcon task for each set bit
 * 0-9 of gGameState.saveFile->unlocks and runs a 6-item menu (callback MakeUp_MenuItem) on
 * gHelirins[0] until B (restores type/color/items, sound 0x22) or a choice (sound 0x21,
 * Save_Write) ends it. START moves the cursor to item 4.
 */
void MakeUp_Run(void)
{
    struct Menu menu;
    struct SaveFile *p;
    const struct TileCopy *e;
    const void *const *src;
    u8 *dst;
    struct MakeUpIcon *task;
    s32 state;
    s32 i;

    state = 0;
    p = gGameState.saveFile;
    do
    {
        if (state == 0)
        {
            Bg_LoadScreen(0xC);
            Sprite_ClearLists();
            Sprite_BuildOam();
            Main_EndFrame();
            Main_QueueCopy(gUnk_083A7C98, (void *)0x060174A0, 0x680);
            Main_QueueCopy(gMakeUpObjPalettes, (void *)0x05000200, 0x1C0);
            for (e = gMakeUpSpriteTiles; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            dst = (u8 *)0x06012400;
            src = gMakeUpIconTiles;
            for (i = 21; i >= 0; i--)
            {
                Main_QueueCopy(*src++, dst, 0x80);
                dst += 0x80;
            }
            menu.func = MakeUp_MenuItem;
            menu.itemCount = 5;
            menu.current = 1;
            menu.x = 0x10;
            menu.y = 0x28;
            menu.unkC = 0;
            menu.unkE = 0;
            menu.unk10 = p->type;
            menu.unk12 = p->color;
            menu.unk14 = p->items;
            Task_Init();
            gGameState.mode = 7;
            gGameState.paused = 0;
            gGameState.helirin = &gHelirins[0];
            Helirin_Init(&gHelirins[0], 0);
            POS_PIXEL(gHelirins[0].x) = 0xC0;
            POS_PIXEL(gHelirins[0].y) = 0x40;
            gCameraX = 0;
            gCameraY = 0;
            Intro_StartBackgroundTasks();
            for (i = 0; i < 10; i++)
            {
                if (p->unlocks & (1 << i))
                {
                    task = (struct MakeUpIcon *)Task_Create(Bonus_UpdateMakeUpIcon, sizeof(*task));
                    if (task != NULL)
                    {
                        task->item = i;
                        task->tile = 0x164 + i * 4;
                        task->palette = gMakeUpIconPalettes[i];
                        task->x = 8 + i * 0x18;
                        task->y = 0x80;
                    }
                }
            }
            MakeUp_StartWornItems();
            Sprite_InitRotations();
            state = 1;
        }
        else if (state == 1)
        {
            Sprite_ClearLists();
            if (gMain.newKeys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                p->type = menu.unk10;
                p->color = menu.unk12;
                p->items = menu.unk14;
                state = -1;
            }
            if (gMain.newKeys & START_BUTTON)
            {
                m4aSongNumStart(0x21);
                menu.current = 4;
                menu.unkC = 7;
            }
            if (Menu_RunWrap(&menu, gMain.repeatKeys))
            {
                m4aSongNumStart(0x21);
                Save_Write();
                state = -1;
            }
            Task_RunAll();
            Helirin_UpdatePreview(&gHelirins[0]);
            Sprite_BuildOam();
        }
        Main_EndFrame();
    } while (state != -1);
}
/**
 * The callback of MakeUp_Run's menu (see Menu_RunWrap). Items 1-3 are rows of five
 * choices (bits 0x11.. of gGameState.saveFile->unlocks unlock them; A stores the choice in
 * type and gHelirins[0].type), item 4 a row of seven (bits 10..16; color and
 * gHelirins[0].color) plus an exit at column 7 (returns 1), item 5 toggles bits 0-9 of
 * items (bits 0-9 of unlocks unlock them; turning one on starts a Bonus_UpdateWornItem task).
 * 0x100-0x104 draw the rows, 0xFFFF the cursor. A locked choice plays sound 0x36.
 */
s32 MakeUp_MenuItem(struct Menu *menu, s32 item)
{
    struct Helirin *h = &gHelirins[0];
    struct SaveFile *q = gGameState.saveFile;
    struct TextCursor *l;
    struct Sprite *spr;
    struct WornItem *task;
    s32 x;
    s32 y;
    s32 bit;
    s32 i;

    switch (item)
    {
    case 1:
    case 2:
    case 3:
        menu->unkE = gMakeUpTypeColumns[menu->unkC];
        if (Menu_StepValueWrap(&menu->unkE, 4, gMain.repeatKeys))
            menu->unkC = menu->unkE;
        if (gMain.newKeys & A_BUTTON)
        {
            menu->unkC = menu->unkE;
            if (q->unlocks & (1 << ((item - 1) * 5 + menu->unkC + 0x11)))
            {
                m4aSongNumStart(0x21);
                h->type = q->type = (menu->current - 1) * 5 + (u8)menu->unkC;
                Helirin_SetSize(h);
            }
            else
            {
                m4aSongNumStart(0x36);
            }
        }
        break;
    case 4:
        menu->unkE = gMakeUpColorColumns[menu->unkC];
        if (Menu_StepValueWrap(&menu->unkE, 7, gMain.repeatKeys))
            menu->unkC = menu->unkE;
        if (gMain.newKeys & A_BUTTON)
        {
            menu->unkC = menu->unkE;
            if (menu->unkC == 7)
                return 1;
            if (q->unlocks & (1 << (menu->unkC + 10)))
            {
                m4aSongNumStart(0x21);
                h->color = q->color = menu->unkC;
                Helirin_SetColor(h, h->color);
            }
            else
            {
                m4aSongNumStart(0x36);
            }
        }
        break;
    case 5:
        Menu_StepValueWrap(&menu->unkC, 9, gMain.repeatKeys);
        menu->unkE = menu->unkC;
        if (gMain.newKeys & A_BUTTON)
        {
            if (q->unlocks & (1 << menu->unkC))
            {
                m4aSongNumStart(0x21);
                q->items ^= 1 << menu->unkC;
                if ((q->items >> menu->unkC) & 1)
                {
                    task = (struct WornItem *)Task_Create(Bonus_UpdateWornItem, sizeof(*task));
                    if (task != NULL)
                    {
                        task->item = menu->unkC;
                        task->point = menu->unkC + 1;
                    }
                }
            }
            else
            {
                m4aSongNumStart(0x36);
            }
        }
        break;
    case 0x100:
        l = &gTextCursor;
        l->x = menu->x;
        l->y = menu->y;
        l->dx = 0x18;
        l->dy = 0x18;
        l->palette = 0xE;
        l->list = 1;
        gMakeUpIconTile = 0x120;
        break;
    case 0x101:
    case 0x102:
    case 0x103:
        l = &gTextCursor;
        bit = (item - 0x101) * 5 + 0x11;
        for (i = 4; i >= 0; i--)
        {
            if (q->unlocks & (1 << bit))
            {
                spr = Sprite_Alloc(7);
                if (spr != NULL)
                {
                    spr->attr01 = (((l->x - 8) << 16) & 0x01FF0000) | 0x40000000 | (u8)(l->y - 8);
                    spr->attr2 = gMakeUpIconTile | 0x5400;
                }
            }
            if (q->type == bit - 0x11)
            {
                spr = Sprite_Alloc(8);
                if (spr != NULL)
                {
                    spr->attr01 = (((l->x - 0x10) << 16) & 0x01FF0000) | 0x80000000 | (u8)(l->y - 0x10);
                    spr->attr2 = 0xE75D;
                }
            }
            gMakeUpIconTile += 4;
            bit++;
            l->x += l->dx;
        }
        l->x = menu->x;
        l->y += l->dy;
        break;
    case 0x104:
        l = &gTextCursor;
        for (bit = 10, i = 0; i < 7; bit++, i++)
        {
            if (q->unlocks & (1 << bit))
            {
                spr = Sprite_Alloc(7);
                if (spr != NULL)
                {
                    spr->attr01 = (((l->x - 8) << 16) & 0x01FF0000) | 0x40000000 | (u8)(l->y - 8);
                    spr->attr2 = gMakeUpIconTile | (gMakeUpColorPalettes[i] << 12) | 0x400;
                }
            }
            if (q->color == i)
            {
                spr = Sprite_Alloc(8);
                if (spr != NULL)
                {
                    spr->attr01 = (((l->x - 0x10) << 16) & 0x01FF0000) | 0x80000000 | (u8)(l->y - 0x10);
                    spr->attr2 = 0xE75D;
                }
            }
            gMakeUpIconTile += 4;
            l->x += l->dx;
        }
        spr = Sprite_Alloc(l->list);
        if (spr != NULL)
        {
            spr->attr01 = (l->x << 16) | 0x40000000 | (u8)(l->y - 8);
            spr->attr2 = 0xE3AD;
        }
        spr = Sprite_Alloc(l->list);
        if (spr != NULL)
        {
            spr->attr01 = ((l->x + 0x10) << 16) | 0x40000000 | (u8)(l->y - 8);
            spr->attr2 = 0xE3B1;
        }
        l->x = menu->x;
        l->y += l->dy;
        break;
    case 0xFFFF:
        // OBJ palette 15, colour 15 cycles every 4 frames
        *(vu16 *)0x050003DE = *gMenuCursorColors[(gMain.frameCount >> 2) & 3];
        l = &gTextCursor;
        x = l->dx * menu->unkE + l->x;
        y = l->y - 0x10;
        if (!(menu->current == 4 && menu->unkE == 7)) // the exit has no offset
            x -= 0x10;
        spr = Sprite_Alloc(8);
        if (spr != NULL)
        {
            spr->attr01 = ((x << 16) & 0x01FF0000) | 0x80000000 | (u8)y;
            spr->attr2 = 0xE74D;
        }
        break;
    }
    return 0;
}
s32 Lives_SettingsMenuItem(struct Menu *p, s32 item);
/**
 * A settings screen nothing references, with its own frame loop: a menu (Lives_SettingsMenuItem)
 * over a copy of gLivesSettings: unkE is bit 0 of invincible, unk10 and unk12 are lives and
 * challengeLives minus 1, unk14 a song number. A on item 4 resets the copy from
 * gDefaultLivesSettings, A on item 5 plays the song, A on item 6 or Start stores the copy
 * in gLivesSettings and leaves, B leaves without storing it. Stops all music on leaving.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
void Lives_SettingsScreen(void)
{
    struct Menu menu;
    s32 state = 0;

    do
    {
        switch (state)
        {
        case 0:
            menu.func = Lives_SettingsMenuItem;
            menu.itemCount = 6;
            menu.current = 1;
            menu.x = 0x40;
            menu.y = 0x20;
            menu.unkE = gLivesSettings.invincible & 1;
            menu.unk10 = gLivesSettings.lives - 1;
            menu.unk12 = gLivesSettings.challengeLives - 1;
            menu.unk14 = 0;
            state = 1;
            break;
        case 1:
            Sprite_ClearLists();
            Menu_RunWrap(&menu, gMain.newKeys);
            Sprite_BuildOam();
            if (gMain.newKeys & B_BUTTON)
            {
                state = -1;
                break;
            }
            if ((gMain.newKeys & A_BUTTON) && menu.current == 4)
            {
                menu.unkE = gDefaultLivesSettings.invincible & 1;
                menu.unk10 = gDefaultLivesSettings.lives - 1;
                menu.unk12 = gDefaultLivesSettings.challengeLives - 1;
            }
            if ((gMain.newKeys & A_BUTTON) && menu.current == 5)
                m4aSongNumStart(menu.unk14);
            if (((gMain.newKeys & A_BUTTON) && menu.current == 6) || (gMain.newKeys & START_BUTTON))
            {
                gLivesSettings.invincible = (gLivesSettings.invincible & ~1) | (menu.unkE & 1);
                gLivesSettings.lives = menu.unk10 + 1;
                gLivesSettings.challengeLives = menu.unk12 + 1;
                state = -1;
            }
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    m4aMPlayAllStop();
}
/**
 * Lives_SettingsScreen's menu function (Menu_Run's items: 0x100 sets up gTextCursor,
 * 0x101-0x106 draw the six lines, 1-6 handle the current one; the current line in palette
 * 15, the others 14). The lines (katakana in the ROM): invincibility ON/OFF (unkE, the
 * strings of gUnk_080C487C), Adventure HP (unk10 + 1, 1-6), Challenge HP (unk12 + 1),
 * the default settings, a sound test (song unk14, 0-99, with the repeating keys), and
 * "set". Always returns 0.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Lives_SettingsMenuItem(struct Menu *p, s32 item)
{
    if (item == p->current + 0x100)
        gTextCursor.palette = 15;
    else
        gTextCursor.palette = 14;

    switch (item)
    {
    case 0x100:
        gTextCursor.x = p->x;
        gTextCursor.y = p->y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 14;
        gTextCursor.list = 0;
        break;
    case 0x101:
        Text_Print(&gTextCursor, gUnk_0801ED08);
        Text_Print(&gTextCursor, gUnk_080C487C[p->unkE]);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        Menu_StepValueWrap(&p->unkE, 1, gMain.newKeys);
        break;
    case 0x102:
        Text_Print(&gTextCursor, gUnk_0801ED10);
        Text_DrawNumber(gTextCursor.x, gTextCursor.y, p->unk10 + 1, gTextCursor.palette);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 2:
        Menu_StepValueWrap(&p->unk10, 5, gMain.newKeys);
        break;
    case 0x103:
        Text_Print(&gTextCursor, gUnk_0801ED1C);
        Text_DrawNumber(gTextCursor.x, gTextCursor.y, p->unk12 + 1, gTextCursor.palette);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 3:
        Menu_StepValueWrap(&p->unk12, 5, gMain.newKeys);
        break;
    case 0x104:
        Text_Print(&gTextCursor, gUnk_0801ED28);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x105:
        Text_Print(&gTextCursor, gUnk_0801ED38);
        Text_DrawNumber(gTextCursor.x, gTextCursor.y, p->unk14, gTextCursor.palette);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 5:
        Menu_StepValueWrap(&p->unk14, 99, gMain.repeatKeys);
        break;
    case 0x106:
        Text_Print(&gTextCursor, gUnk_0801ED40);
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    }
    return 0;
}
