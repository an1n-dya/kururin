// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// files: the file select, name entry and delete-data screens.
// 0800B8A0-0800C75C, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "task.h"
#include "menu.h"
#include "level.h"
#include "save.h"
#include "intro.h"
#include "sound.h"

s32 Files_FileMenuItem(struct Menu *p, s32 item);
s32 Files_EnterName(char *name);
s32 Files_DeleteScreen(void);
extern const u8 gFileMenuFiles[5];
extern const u8 gTextNoSavedData[];
extern const u8 gTextEasy[];
extern const u8 gTextNormal[];
extern const u8 gTextEnd[];
extern const u8 gTextDeleteSavedData[];
extern const u8 *const gWorldShortNames[];

/**
 * The file select screen (gMain.state 2), until the state changes: step gMain.subState 0 sets up
 * the screen and the file menu (Files_FileMenuItem, items 1-6); step 1 runs the menu: B (song
 * 0x22) goes back to state 1, A (song 0x21) on files 1-5 picks gFileMenuFiles's save file as
 * gGameState.file/saveFile, clears gGameState.mode and flags, and goes to state 3, or for an
 * empty file to step 2 (Files_EnterName on its name: 0 goes on to state 3, else back to step 1);
 * item 6 runs step 3 (Files_DeleteScreen, then step 1).
 */
void Files_MainState(void)
{
    struct Menu menu;
    u16 keys;
    s32 n;
    u8 file;

    gMain.subState = 0;
    while (gMain.state == 2)
    {
        switch ((s8)gMain.subState)
        {
        case 0:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_SetVideoMode(4);
            gBgLayers[0].hofs = 0;
            gBgLayers[0].vofs = 0;
            Sprite_ClearLists();
            Sprite_BuildOam();
            Main_QueueCopy(gMenuObjTiles, (void *)0x06012400, 0x4680);
            Main_QueueCopy(gMenuObjPalettes, (void *)0x05000200, 0x1C0);
            Main_QueueCopy(gTextPalettes, (void *)0x050003C0, 0x40);
            Main_QueueCopy(gTextPalettes, (void *)0x050001C0, 0x40);
            Bg_LoadScreen(1);
            gDispcntCopy = (gDispcntCopy & ~0x100) | 0x1600;
            Task_Init();
            Intro_StartBackgroundTasks();
            Sprite_InitRotations();
            gGameState.world = 0;
            gGameState.level = 0;
            gGameState.area = 0;
            menu.func = Files_FileMenuItem;
            menu.itemCount = 6;
            menu.current = 1;
            menu.x = 0x20;
            menu.y = 0xC;
            Sound_PlaySongOrContinue(0xE);
            gMain.subState = 1;
            break;
        case 1:
            gDispcntCopy &= ~0x100;
            Sprite_ClearLists();
            Menu_RunWrap(&menu, gMain.newKeys);
            keys = gMain.newKeys;
            if (keys & B_BUTTON)
            {
                m4aSongNumStart(0x22);
                gMain.state = 1;
            }
            else if (keys & A_BUTTON)
            {
                m4aSongNumStart(0x21);
                n = menu.current - 1;
                if (n >= 0)
                {
                    if (n <= 4)
                    {
                        file = gFileMenuFiles[n];
                        gGameState.file = file;
                        gGameState.saveFile = (struct SaveFile *)(gSaveFiles + file * 0x1F4);
                        gGameState.mode = 0;
                        gGameState.flags = 0;
                        if (gGameState.saveFile->name[0] == '\0')
                            gMain.subState = 2;
                        else
                            gMain.state = 3;
                    }
                    else if (n == 5)
                    {
                        gMain.subState = 3;
                    }
                }
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 2:
            if (Files_EnterName(gGameState.saveFile->name) == 0)
                gMain.state = 3;
            else
                gMain.subState = 1;
            Bg_LoadScreen(1);
            break;
        case 3:
            Files_DeleteScreen();
            gMain.subState = 1;
            break;
        }
        Main_EndFrame();
    }
}
/**
 * Draws item *item* of the file menu *p* (MakeUp_MenuItem's protocol) at the cursor
 * gTextCursor, palette 15, or 14 for the current item while bit 3 of the frame counter is
 * set: 0x100 sets the cursor up at (p->x, p->y), 8 x 0x18 per character and row;
 * 0x101-0x105 draw save file gFileMenuFiles[item - 0x101] (the 0x1F4-byte records at
 * gSaveFiles): gTextNoSavedData when it is empty, else its name and, when its flags have
 * bit 0, gTextEasy or gTextNormal by bit 4 and the level after its last one with a mark
 * (Level_SetFromIndex): the world's name (gWorldShortNames) and level, or gTextEnd beyond
 * world 10 (11 with flags bit 1); then a new row. 0x106 draws gTextDeleteSavedData 8 pixels
 * lower. Returns 0.
 */
s32 Files_FileMenuItem(struct Menu *p, s32 item)
{
    struct TextCursor *cur;
    struct SaveFile *file;
    s32 n;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;
    cur = &gTextCursor;

    switch (item)
    {
    case 0x100:
        cur->x = p->x;
        cur->y = p->y;
        cur->dx = 8;
        cur->dy = 0x18;
        cur->palette = 14;
        cur->list = 4;
        break;
    case 0x101:
    case 0x102:
    case 0x103:
    case 0x104:
    case 0x105:
        file = (struct SaveFile *)(gSaveFiles + gFileMenuFiles[item - 0x101] * 0x1F4);
        if (file->name[0] == '\0')
        {
            Text_PrintProportional(&gTextCursor, gTextNoSavedData);
        }
        else
        {
            Text_DrawName(gTextCursor.x, gTextCursor.y, file->name, file->rank, gTextCursor.palette);
            if (file->flags & 1)
            {
                gTextCursor.x = p->x + 0x50;
                if (file->flags & 0x10)
                    Text_PrintProportional(&gTextCursor, gTextEasy);
                else
                    Text_PrintProportional(&gTextCursor, gTextNormal);
                gTextCursor.x = p->x + 0x80;
                for (n = 0x25; n >= 0 && file->marks[n] == 0; n--)
                    ;
                Level_SetFromIndex(n + 1);
                if ((file->flags & 2) ? gGameState.world <= 11 : gGameState.world <= 10)
                {
                    Text_PrintProportional(&gTextCursor, gWorldShortNames[gGameState.world]);
                    Text_DrawNumber(gTextCursor.x, gTextCursor.y, gGameState.level + 1,
                                 gTextCursor.palette);
                }
                else
                {
                    Text_PrintProportional(&gTextCursor, gTextEnd);
                }
            }
        }
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x106:
        cur->y += 8;
        Text_Print(cur, gTextDeleteSavedData);
        break;
    }
    return 0;
}
// The name entry menu of Files_EnterName: its Menu_RunWrap menu and the name it edits, which
// the item callback Files_NameEntryItem reads at +0x1C.
struct NameEntryMenu {
    /*0x00*/ struct Menu menu;
    /*0x18*/ u32 unk18;
    /*0x1C*/ char *name;
};
// Files_EnterName: the names it offers on Start (by the frame counter's low 2 bits)
extern const u8 *const gDefaultNames[4];
s32 Files_NameEntryItem(struct Menu *p, s32 item);

/**
 * The name entry screen with its own frame loop: clears *name* to eight '_' and runs a
 * menu of 7 x 2 items (Menu_RunWrap, items by Files_NameEntryItem) on the repeated keys. Start
 * (while menu.unk12, the name's length, is 0) fills in one of the four names of
 * gDefaultNames. When the menu returns non-zero the name ends at its length; a positive
 * result returns 0, a negative one is returned.
 */
s32 Files_EnterName(char *name)
{
    struct NameEntryMenu m;
    s32 state = 0;
    s32 result = 0;
    s32 i;

    do
    {
        switch (state)
        {
        case 0:
            Bg_LoadScreen(6);
            Main_QueueCopy(gUnk_083A7C98, (void *)0x060174A0, 0x680);
            Main_QueueCopy(gMenuCursorTiles, (void *)0x06016A80, 0x200);
            m.menu.func = Files_NameEntryItem;
            m.menu.itemCount = 7;
            m.menu.current = 2;
            m.menu.x = 0x40;
            m.menu.y = 0x0C;
            m.menu.unkE = 0;
            m.menu.unk10 = 0;
            m.menu.unk12 = 0;
            m.menu.unk14 = 0;
            m.name = name;
            for (i = 7; i >= 0; i--)
                name[i] = '_';
            name[8] = 0;
            state = 1;
            break;
        case 1:
            Sprite_ClearLists();
            if (gMain.newKeys & START_BUTTON)
            {
                m4aSongNumStart(0x21);
                m.menu.current = 2;
                m.menu.unk10 = 10;
                if (m.menu.unk12 == 0)
                {
                    i = Text_Convert(gDefaultNames[gMain.frameCount & 3], (u8 *)name);
                    m.menu.unk12 = i;
                    for (; i <= 7; i++)
                        name[i] = '_';
                    name[i] = 0;
                }
            }
            result = Menu_RunWrap(&m.menu, gMain.repeatKeys);
            if (result != 0)
            {
                if (result > 0)
                    result = 0;
                name[m.menu.unk12] = 0;
                Save_Write();
                state = -1;
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
// The name entry's characters: [kind][row] a string per row of the grid (kind 2's start with
// the 2-byte "#h" escape); its columns of the kind row (unk10 -> unk14) and back
extern const u8 *const gNameEntryRows[3][6];
extern const u8 gNameEntryColumnKinds[];
extern const u8 gNameEntryKindColumns[];

// The name entry's cursor: a 32x32 sprite (tile 0x354) at (x, y)
#define NAME_CURSOR(x, y)                                                              \
    {                                                                                  \
        struct Sprite *cur = Sprite_Alloc((u8)(gTextCursor.list + 1));         \
        if (cur != NULL)                                                               \
        {                                                                              \
            cur->attr01 = (((x) << 16) & 0x1FF0000) | 0x80000000 | (u8)(y);              \
            cur->attr2 = ((gTextCursor.list >> 2) << 10) | 0xE354;                    \
        }                                                                              \
    }

/**
 * The items of the name entry menu (Files_EnterName; p is its struct NameEntryMenu). Drawing:
 * 0x100 the name, 0x101 the row of the three kinds (16x16 sprites, the cursor on kind unk14
 * in item 1), 0x102-0x107 the six rows of the grid of kind unkE (0x102 and 0x103 with their
 * labels on the left), the cursor on column unk10 of the current row (10: the label);
 * 0xFFFF the cursor's blinking colour. Keys: in each item L/R change the kind (Menu_StepValueLRWrap)
 * and left/right the column (Menu_StepValueWrap: items 1 among the kinds, 2 and 3 among 10
 * columns and the label, 4-7 among 10 columns). A enters the character (unkE/row/unk10)
 * while the name is shorter than 8 (kind 2 moves 0xA6-0xDD as Text_Convert's "#h" does);
 * A on item 2's label ends (returns 1) unless the name is only spaces (song 0x36); A on item
 * 3's label and B delete the last character; B on an empty name returns -1.
 */
s32 Files_NameEntryItem(struct Menu *p, s32 item)
{
    char *name = ((struct NameEntryMenu *)p)->name;
    struct Sprite *spr;
    s32 x, y;
    u32 len;
    s32 kind;
    u32 c;
    s32 i;

    switch (item)
    {
    case 0xFFFF:
        *(vu16 *)0x050003DE = *gMenuCursorColors[(gMain.frameCount >> 2) & 3];
        return 0;
    case 0x100:
        gTextCursor.x = p->x + 0x38;
        gTextCursor.y = p->y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 0xE;
        gTextCursor.list = 1;
        Text_Print(&gTextCursor, (const u8 *)name);
        gTextCursor.x = p->x;
        gTextCursor.y += 0x18;
        return 0;
    case 0x101:
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = 0xE3BD;
        }
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x10) << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = 0xE3C1;
        }
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = 0xE3C5;
        }
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x50) << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = 0xE3C9;
        }
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x60) << 16) | 0x40000000 | gTextCursor.y;
            spr->attr2 = 0xE3CD;
        }
        gTextCursor.x = p->x;
        if (p->current == 1)
        {
            x = p->unk14 * 80 + gTextCursor.x;
            y = gTextCursor.y - 8;
            NAME_CURSOR(x, y)
        }
        gTextCursor.dx = 0x10;
        gTextCursor.y += 0x10;
        return 0;
    case 0x102:
    case 0x103:
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = (((gTextCursor.x - 0x30) << 16) & 0x1FF0000) | 0x40000000 | gTextCursor.y;
            spr->attr2 = item == 0x102 ? 0xE3AD : 0xE3B5;
        }
        spr = Sprite_Alloc(gTextCursor.list);
        if (spr != NULL)
        {
            spr->attr01 = (((gTextCursor.x - 0x20) << 16) & 0x1FF0000) | 0x40000000 | gTextCursor.y;
            spr->attr2 = item == 0x102 ? 0xE3B1 : 0xE3B9;
        }
        // fall through
    case 0x104:
    case 0x105:
    case 0x106:
    case 0x107:
        Text_Print(&gTextCursor, gNameEntryRows[p->unkE][item - 0x102]);
        gTextCursor.x = p->x;
        if (item - 0x100 == p->current)
        {
            if (p->unk10 == 10)
                x = gTextCursor.x - 0x30;
            else
                x = p->unk10 * 16 + gTextCursor.x - 0xC;
            y = gTextCursor.y - 8;
            NAME_CURSOR(x, y)
        }
        gTextCursor.y += 0x10;
        return 0;
    case 1:
        p->unk14 = gNameEntryColumnKinds[p->unk10];
        Menu_StepValueLRWrap(&p->unkE, 1, gMain.newKeys);
        if (Menu_StepValueWrap(&p->unk14, 1, gMain.repeatKeys))
            p->unk10 = gNameEntryKindColumns[p->unk14];
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            p->unkE = p->unk14;
        }
        goto back;
    case 2:
        Menu_StepValueLRWrap(&p->unkE, 1, gMain.newKeys);
        Menu_StepValueWrap(&p->unk10, 10, gMain.repeatKeys);
        if (p->unk10 != 10)
            goto enter;
        if (!(gMain.newKeys & A_BUTTON))
            goto back;
        // the end, unless the name is empty or only spaces
        for (i = 0; i < p->unk12; i++)
        {
            if (name[i] != ' ')
            {
                m4aSongNumStart(0x21);
                return 1;
            }
        }
        m4aSongNumStart(0x36);
        return 0;
    case 3:
        Menu_StepValueLRWrap(&p->unkE, 1, gMain.newKeys);
        Menu_StepValueWrap(&p->unk10, 10, gMain.repeatKeys);
        if (p->unk10 != 10)
            goto enter;
        if (!(gMain.newKeys & A_BUTTON))
            goto back;
        m4aSongNumStart(0x21);
        if (p->unk12 == 0)
            return 0;
        goto delete;
    case 4:
    case 5:
    case 6:
    case 7:
        if (p->unk10 > 9)
            p->unk10 = 0;
        Menu_StepValueLRWrap(&p->unkE, 1, gMain.newKeys);
        Menu_StepValueWrap(&p->unk10, 9, gMain.repeatKeys);
        goto enter;
    default:
        return 0;
    }

enter:
    if (gMain.newKeys & A_BUTTON)
    {
        m4aSongNumStart(0x21);
        len = p->unk12;
        if (len > 7)
            return 0;
        kind = p->unkE;
        if (kind == 0 || kind == 1)
        {
            p->unk12 = len + 1;
            name[len] = gNameEntryRows[kind][item - 2][p->unk10];
        }
        else if (kind == 2)
        {
            // after its "#h"
            c = gNameEntryRows[2][item - 2][p->unk10 + 2];
            if ((u8)(c - 0xA6) <= 0x37)
            {
                if (c <= 0xBF)
                {
                    p->unk12 = len + 1;
                    name[len] = c - 0x20;
                }
                else
                {
                    p->unk12 = len + 1;
                    name[len] = c + 0x20;
                }
            }
            else
            {
                p->unk12 = len + 1;
                name[len] = c;
            }
        }
    }
back:
    if (!(gMain.newKeys & B_BUTTON))
        return 0;
    m4aSongNumStart(0x22);
    if (p->unk12 == 0)
        return -1;
delete:
    p->unk12--;
    name[p->unk12] = '_';
    return 0;
}
s32 Files_DeleteMenuItem(struct Menu *p, s32 item);
s32 Files_DeleteConfirmItem(struct Menu *p, s32 item);

/**
 * The "delete saved data" screen, with its own frame loop: a menu of the five save files
 * (Files_DeleteMenuItem); B leaves, and so does A on an empty file. A on a file opens a second menu
 * about it (Files_DeleteConfirmItem, starting on its second item): A there clears the file
 * (Save_ResetFile) and saves (Save_Write); either way the screen ends. Returns 0.
 */
s32 Files_DeleteScreen(void)
{
    struct Menu files;
    struct Menu confirm;
    s32 state = 0;
    s32 result;

    do
    {
        switch (state)
        {
        case 0:
            files.func = Files_DeleteMenuItem;
            files.itemCount = 5;
            files.current = 1;
            files.x = 0x20;
            files.y = 0x0C;
            state = 1;
            // fall through
        case 1:
            Sprite_ClearLists();
            result = Menu_RunWrap(&files, gMain.newKeys);
            if (result == -1)
            {
                state = -1;
            }
            else if (result == 1)
            {
                state = 2;
                if (((struct SaveFile *)(gSaveFiles + gFileMenuFiles[files.current - 1] * 0x1F4))->name[0] == '\0')
                    state = -1;
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 2:
            confirm.func = Files_DeleteConfirmItem;
            confirm.itemCount = 2;
            confirm.current = 2;
            confirm.x = 0x20;
            confirm.y = 0x0C;
            confirm.unkC = gFileMenuFiles[files.current - 1];
            state = 3;
            // fall through
        case 3:
            Sprite_ClearLists();
            result = Menu_RunWrap(&confirm, gMain.newKeys);
            if (result == -1)
            {
                state = -1;
            }
            else if (result == 1)
            {
                Save_ResetFile(confirm.unkC);
                Save_Write();
                state = -1;
            }
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    return 0;
}
extern const u8 gTextDeleteMenuTitle[];

/**
 * Like Files_FileMenuItem, the items of a file menu (Files_DeleteScreen's) at the cursor gTextCursor:
 * 0x100 sets the cursor up, 0x101-0x105 draw save file gFileMenuFiles[item - 0x101] and
 * 0x1FF draws gTextDeleteMenuTitle 8 pixels lower. The current item (1-5) returns -1 on B (song
 * 0x22) and 1 on A (song 0x21); everything else returns 0.
 */
s32 Files_DeleteMenuItem(struct Menu *p, s32 item)
{
    struct TextCursor *cur;
    struct SaveFile *file;
    s32 n;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;
    cur = &gTextCursor;

    switch (item)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        if (gMain.newKeys & B_BUTTON)
        {
            m4aSongNumStart(0x22);
            return -1;
        }
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 1;
        }
        break;
    case 0x100:
        cur->x = p->x;
        cur->y = p->y;
        cur->dx = 8;
        cur->dy = 0x18;
        cur->palette = 14;
        cur->list = 4;
        break;
    case 0x101:
    case 0x102:
    case 0x103:
    case 0x104:
    case 0x105:
        file = (struct SaveFile *)(gSaveFiles + gFileMenuFiles[item - 0x101] * 0x1F4);
        if (file->name[0] == '\0')
        {
            Text_PrintProportional(&gTextCursor, gTextNoSavedData);
        }
        else
        {
            Text_DrawName(gTextCursor.x, gTextCursor.y, file->name, file->rank, gTextCursor.palette);
            if (file->flags & 1)
            {
                gTextCursor.x = p->x + 0x50;
                if (file->flags & 0x10)
                    Text_PrintProportional(&gTextCursor, gTextEasy);
                else
                    Text_PrintProportional(&gTextCursor, gTextNormal);
                gTextCursor.x = p->x + 0x80;
                for (n = 0x25; n >= 0 && file->marks[n] == 0; n--)
                    ;
                Level_SetFromIndex(n + 1);
                if ((file->flags & 2) ? gGameState.world <= 11 : gGameState.world <= 10)
                {
                    Text_PrintProportional(&gTextCursor, gWorldShortNames[gGameState.world]);
                    Text_DrawNumber(gTextCursor.x, gTextCursor.y, gGameState.level + 1,
                                 gTextCursor.palette);
                }
                else
                {
                    Text_PrintProportional(&gTextCursor, gTextEnd);
                }
            }
        }
        gTextCursor.x = p->x;
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x1FF:
        cur->y += 8;
        cur->palette = 14;
        Text_Print(cur, gTextDeleteMenuTitle);
        break;
    }
    return 0;
}
extern const u8 gTextDeleteConfirm1[];
extern const u8 gTextDeleteConfirm2[];
extern const u8 gTextDeleteYes[];
extern const u8 gTextDeleteNo[];

/**
 * The items of a menu about save file p->unkC (Files_DeleteScreen's second menu) at the cursor
 * gTextCursor: 0x100 sets the cursor up and draws the file's name and the texts
 * gTextDeleteConfirm1 and gTextDeleteConfirm2 a row apart, 0x101 / 0x102 draw gTextDeleteYes /
 * gTextDeleteNo one row lower each. B on item 1 or 2 returns -1 (song 0x22); A returns 1 on
 * item 1 and -1 on item 2 (song 0x21); else 0.
 */
s32 Files_DeleteConfirmItem(struct Menu *p, s32 item)
{
    struct TextCursor *cur;
    struct SaveFile *file;

    if ((gMain.frameCount & 8) && item == p->current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;
    cur = &gTextCursor;

    switch (item)
    {
    case 0x100:
        cur->x = p->x;
        cur->y = p->y;
        cur->dx = 8;
        cur->dy = 0x18;
        cur->palette = 14;
        cur->list = 4;
        file = (struct SaveFile *)(gSaveFiles + p->unkC * 0x1F4);
        Text_DrawName(cur->x, cur->y, file->name, file->rank, 14);
        cur->x = p->x;
        cur->y += cur->dy;
        Text_Print(cur, gTextDeleteConfirm1);
        cur->x = p->x;
        cur->y += cur->dy;
        Text_Print(cur, gTextDeleteConfirm2);
        cur->y += cur->dy;
        break;
    case 0x101:
        cur->x = p->x;
        Text_PrintProportional(cur, gTextDeleteYes);
        cur->y += cur->dy;
        break;
    case 0x102:
        cur->x = p->x;
        Text_PrintProportional(cur, gTextDeleteNo);
        cur->y += cur->dy;
        break;
    case 1:
        if (gMain.newKeys & B_BUTTON)
        {
            m4aSongNumStart(0x22);
            return -1;
        }
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return 1;
        }
        break;
    case 2:
        if (gMain.newKeys & B_BUTTON)
        {
            m4aSongNumStart(0x22);
            return -1;
        }
        if (gMain.newKeys & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return -1;
        }
        break;
    }
    return 0;
}
