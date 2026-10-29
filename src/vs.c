// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// vs: the linked (VS) game: waiting for the consoles, its menus and screens.
// 0801016C-080131F8, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "sprite.h"
#include "text.h"
#include "link.h"
#include "util.h"
#include "menu.h"
#include "vs.h"
#include "helirin.h"
#include "task.h"
#include "effects.h"
#include "sound.h"
#include "bg.h"
#include "decompress.h"

extern u8 gVsCourseOrder[55]; // a permutation of 0..54 (Vs_ShuffleCourses), swapped by Vs_NextCourse
extern s32 gVsCourseIndex; // index into gVsCourseOrder (Vs_NextCourse)
u8 Vs_NextCourse(void);

extern const u8 gTextIdError[];
extern const u8 gTextTimeOut[];

/**
 * A frame of waiting for the linked consoles (ModeMenu_Run after the MultiBoot transfer):
 * "ID ERROR!" (gTextIdError) and -1 when SIOCNT's player number is no longer
 * gLinkPlayer. While the consoles that answer (gLinkStatus & 0xF) are not all of
 * gLinkPlayers, counts gLinkWaitFrames up and returns 1, for up to 0x3C frames. Once they
 * are, gLinkTimer counts the frames Link_FindCommand(gLinkPlayers, hi) is still not all
 * of them, and the result is 0. Either count past 0x3C draws "TIME OUT!" (gTextTimeOut)
 * and returns -1.
 */
s32 Vs_WaitForConsoles(u32 hi)
{
    if (((struct SioMultiCnt *)REG_ADDR_SIOCNT)->id != gLinkPlayer)
    {
        Text_DrawSmall(0x58, 0x50, gTextIdError, 14);
        return -1;
    }
    if ((gLinkStatus & 0xF) != gLinkPlayers)
    {
        if (++gLinkWaitFrames <= 0x3C)
            return 1;
    }
    else
    {
        gLinkWaitFrames = 0;
        if (Link_FindCommand(gLinkPlayers, hi) == gLinkPlayers)
            gLinkTimer = 0;
        else
            gLinkTimer++;
        if (gLinkTimer <= 0x3C)
            return 0;
    }
    Text_DrawSmall(0x58, 0x48, gTextTimeOut, 14);
    return -1;
}
/**
 * The same code as Vs_CheckLevelLink, for the linked game's menus (Vs_RunMenus, Vs_RunResults).
 */
s32 Vs_CheckMenuLink(u32 hi)
{
    if (((struct SioMultiCnt *)REG_ADDR_SIOCNT)->id != gLinkPlayer)
    {
        Text_DrawSmall(0x58, 0x50, gTextIdError, 14);
        return -1;
    }
    if (gLinkTimer > 0x3C)
    {
        Text_DrawSmall(0x58, 0x48, gTextTimeOut, 14);
        REG_IE &= 0xFF3F;
        Link_Init((gLinkStatus & 0xF00) >> 8);
        return -1;
    }
    if ((gLinkStatus & 0xF) != gLinkPlayers)
    {
        gLinkTimer++;
        return 1;
    }
    if (Link_FindCommand(gLinkPlayers, hi) != gLinkPlayers)
        gLinkTimer++;
    else
        gLinkTimer = 0;
    return 0;
}
/**
 * Vs_WaitForConsoles during a linked level (Play_RunLevel states 2 and 3): "ID ERROR!" and -1 when
 * the player number changed; "TIME OUT!" and -1 once gLinkTimer has passed 0x3C, which
 * also turns the serial and timer 3 interrupts off and restarts the link (Link_Init with
 * bits 8-11 of gLinkStatus). Otherwise gLinkTimer counts the frames the answering
 * consoles are not all of gLinkPlayers (returns 1) or Link_FindCommand(.., hi) is not all of
 * them (returns 0), and is reset when it is.
 */
s32 Vs_CheckLevelLink(u32 hi)
{
    if (((struct SioMultiCnt *)REG_ADDR_SIOCNT)->id != gLinkPlayer)
    {
        Text_DrawSmall(0x58, 0x50, gTextIdError, 14);
        return -1;
    }
    if (gLinkTimer > 0x3C)
    {
        Text_DrawSmall(0x58, 0x48, gTextTimeOut, 14);
        REG_IE &= 0xFF3F;
        Link_Init((gLinkStatus & 0xF00) >> 8);
        return -1;
    }
    if ((gLinkStatus & 0xF) != gLinkPlayers)
    {
        gLinkTimer++;
        return 1;
    }
    if (Link_FindCommand(gLinkPlayers, hi) != gLinkPlayers)
        gLinkTimer++;
    else
        gLinkTimer = 0;
    return 0;
}
s32 Vs_MatchMenuItem(struct Menu *p, s32 item);
s32 Vs_CourseRowsMenuItem(struct Menu *p, s32 item);
s32 Vs_CourseListMenuItem(struct Menu *p, s32 item);
// Vs_PlayerSettingsItem's menu: a struct Menu up to unkC, then the players' settings.
struct VsSettingsMenu {
    /*0x00*/ s8 unk0;
    /*0x01*/ s8 unk1; // the current item
    /*0x02*/ u8 unk2[0xA];
    /*0x0C*/ u16 settings[4];
};
s32 Vs_PlayerSettingsItem(struct VsSettingsMenu *p, s32 item);
void Vs_ShuffleCourses(void);
void Vs_DrawSpinner(s32 a, s32 b);
extern const struct TileCopy gUnk_0801EB7C[]; // Vs_RunMenus's OBJ tiles
extern const u16 gUnk_08133748[];             // the linked game's OBJ palettes (0x1C0 bytes)
extern const u8 gUnk_080C4844[4];             // the rounds of a match by Vs_MatchMenuItem's unkE: 3, 5, 7, 0 (no end)
extern const u8 gUnk_0801EC1C[];              // "WAIT"

// gLinkSendData as Vs_RunMenus fills it: Link_Send sends 6 words, 12 halfwords
#define SEND (gLinkSendData)
#define VS_SEND8(a, b, c, d, e, f, g, h) \
    do                                   \
    {                                    \
        SEND[0] = (a);                   \
        SEND[1] = (b);                   \
        SEND[2] = (c);                   \
        SEND[3] = (d);                   \
        SEND[4] = (e);                   \
        SEND[5] = (f);                   \
        SEND[6] = (g);                   \
        SEND[7] = (h);                   \
    } while (0)
// a link check of the menu states: -1 (a link error) leaves for state 15, 1 (waiting)
// draws the menu *draw*, anything but 0 ends the frame
#define VS_MENU_LINK(cmd, draw)    \
    switch (Vs_CheckMenuLink(cmd)) \
    {                              \
    case -1:                       \
        state = 15;                \
        goto next;                 \
    case 0:                        \
        break;                     \
    case 1:                        \
        Menu_Draw(draw);           \
        goto next;                 \
    default:                       \
        goto next;                 \
    }
// a receiver (players 1-3 in gGameState.unk2C) asked for the level data again
#define VS_RESEND_ASKED(players)                                     \
    ((((players) & 2) && gLinkRecvData[1].unk4 != 0)                 \
     || (((players) & 4) && gLinkRecvData[2].unk4 != 0)              \
     || (((players) & 8) && gLinkRecvData[3].unk4 != 0))

/**
 * The linked game's menus before a level (mode 4, Play_MainState), with their own frame loop
 * over 17 states, through the link (gLinkSendData / gLinkRecvData). The parent (player 0)
 * runs each menu and sends its state (command 0xN00: the current item and values; 0xN01:
 * B; 0xN02: A), the others send their keys and follow it. *a* 1 starts at the next random
 * course (state 8), 2 at the course list (9), anything else at the beginning:
 * 0:  the screen, the tasks; for a new match mode 4, the player count, Challenge world 12.
 * 1, 2: opens the window circle, then 5. 4: back to the first menu (5).
 * 5:  the first menu (Vs_MatchMenuItem): random rounds (unkE: 3, 5, 7 or endless, gUnk_080C4844)
 *     or the course list; B leaves with -1 (13); A clears the scores and goes to 6 (random
 *     rounds, the courses shuffled) or 9.
 * 6, 7: the course rows menu (Vs_CourseRowsMenuItem: rows unkE to unk10 into gGameState.unk32); B
 *     back to 4, A to 8.
 * 8:  the parent picks the next random course whose row is in unk32 (Vs_NextCourse); 11.
 * 9, 10: the course list (Vs_CourseListMenuItem) with the selected course's minimap; B back to 4,
 *     A sets the course and goes to 11. With MultiBoot guests (gGameState.unk2C) the
 *     parent sends the minimap (its 4-bit pixels as 2 bits each, 0x200 halfwords) along.
 * 11, 12: the player settings (Vs_PlayerSettingsItem: size and lives per player, bit 7:
 *     ready); B there goes back to 6 or 9; once every player is ready the parent sends
 *     0x502 with the course, and the lives and sizes go to gLivesSettings (13, result 0).
 *     With MultiBoot guests the parent sends them the level data first (the 6 halfwords
 *     after the first two of each packet, Vs_DrawSpinner shows the progress, "WAIT").
 * 13, 14: fades out and closes the window circle, then leaves (state -1).
 * 15, 16: a link error: after 0x78 frames closes the circle and leaves with -1.
 * Returns 0 to play the course, -1 to leave.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Vs_RunMenus(s32 a)
{
    struct Menu modeMenu;      // Vs_MatchMenuItem
    struct Menu courseMenu;    // Vs_CourseRowsMenuItem, then Vs_CourseListMenuItem
    union {
        struct Menu menu;
        struct VsSettingsMenu s; // settings[n]: bits 0-3 size, 4-6 lives - 1, 7 ready
    } settingsMenu;            // Vs_PlayerSettingsItem
    u16 *settings = settingsMenu.s.settings;
    s32 state = 0;
    s32 result = 0;
    s32 wait;          // frames the parent waits before it sends while not every player answers
    s32 shown;         // the course whose minimap is drawn
    u32 doneFrames;    // frames the level data has been sent completely (state 12)
    u32 sent;          // the transfer's progress, in halfwords (state 10) or bytes (state 12)
    const u8 *data;    // the level data the parent sends (state 12)
    // The 0x502 send carries these three even when no guest needs the level data (unk2C 0)
    // and nothing set them: the original sends what its stack held there, which no console
    // reads. They start at 0 so the C reads no uninitialised value.
    u32 total = 0;       // the transfer's size
    u32 layer1Start = 0; // where layer 1 starts in the data sent, sent with 0x502
    u32 layer0Size = 0;  // layer 0's decompressed size, sent with 0x502
    u32 got, ready;
    s32 i, k, d, lo, mask, n;
    const struct TileCopy *e;
    const struct LevelEntry *lv;

    do
    {
        switch (state)
        {
        case 0:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_SetVideoMode(4);
            for (e = gUnk_0801EB7C; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            Main_QueueCopy(gUnk_08133748, (void *)0x05000200, 0x1C0);
            Bg_LoadScreen(0x11);
            Task_Init();
            Task_Create(Effect_RotateBg2, 8);
            if (a == 1)
                goto random_course;
            if (a == 2)
                goto course_list;
            gGameState.mode = 4;
            gGameState.playerCount = 0;
            if (gLinkPlayers & 1)
                gGameState.playerCount = 1;
            if (gLinkPlayers & 2)
                gGameState.playerCount++;
            if (gLinkPlayers & 4)
                gGameState.playerCount++;
            if (gLinkPlayers & 8)
                gGameState.playerCount++;
            gGameState.world = 12;
            gGameState.level = 0;
            gDispcntCopy = 0x3741;
            gLinkWaitFrames = 0;
            gLinkTimer = 0;
            state = 1;
            break;
        case 1:
            Sprite_ClearLists();
            Sprite_BuildOam();
            Bg_StartWindowCircle(0, 0x78, 0x50);
            m4aSongNumStart(0x29);
            state = 2;
            goto mode_menu;
        case 4:
            state = 5;
        mode_menu:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_LoadScreen(4);
            modeMenu.func = Vs_MatchMenuItem;
            modeMenu.itemCount = 2;
            modeMenu.current = 1;
            modeMenu.x = 0x40;
            modeMenu.y = 0x40;
            modeMenu.unkC = 0;
            modeMenu.unkE = 0;
            wait = 5;
            Sound_PlaySong(0xE);
            break;
        case 2:
            Bg_DrawWindowCircle(gWindowRadius + 8, gWindowX, gWindowY);
            if (gWindowRadius > 0x9F)
            {
                gDispcntCopy &= 0xDEFF;
                state = 5;
            }
            // fall through
        case 5:
            Sprite_ClearLists();
            if (state != 5)
            {
                Menu_Draw(&modeMenu);
                goto next;
            }
            VS_MENU_LINK(0x200, &modeMenu)
            if (Link_FindCommandExact(1, 0x200, 1) != 0)
            {
                // the parent's B: leave
                result = -1;
                state = 13;
                Menu_Draw(&modeMenu);
                goto next;
            }
            if (Link_FindCommandExact(1, 0x200, 2) != 0)
            {
                // the parent's A
                modeMenu.current = gLinkRecvData[0].unk4;
                modeMenu.unkE = gLinkRecvData[0].unk6;
                gGameState.unk2D = (u8)modeMenu.current - 1;
                if (gGameState.unk2D != 0)
                {
                    gGameState.unk2E = 0;
                    state = 9;
                }
                else
                {
                    gGameState.unk2E = gUnk_080C4844[modeMenu.unkE];
                    Random_Shuffle(gMain.frameCount);
                    Vs_ShuffleCourses();
                    state = 6;
                }
                gGameState.unk30 = 0;
                for (i = 0; i < 4; i++)
                {
                    gGameState.unk34[i] = 0;
                    gGameState.unk3C[i] = 0;
                }
                Menu_Draw(&modeMenu);
                goto next;
            }
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer > 3)
                    goto next;
                VS_SEND8(0x200, 0, gMain.newKeys, 0, 0, 0, 0, 0);
                if (Link_FindCommand(1, 0x200) != 0)
                {
                    modeMenu.current = gLinkRecvData[0].unk4;
                    modeMenu.unkE = gLinkRecvData[0].unk6;
                }
                Menu_Draw(&modeMenu);
                goto next;
            }
            got = Link_FindCommand(gLinkPlayers, 0x200);
            modeMenu.unkC = gMain.newKeys;
            if (got == gLinkPlayers)
            {
                Menu_RunWrap(&modeMenu, modeMenu.unkC);
                if (modeMenu.unkC & B_BUTTON)
                {
                    m4aSongNumStart(0x22);
                    VS_SEND8(0x201, 0, modeMenu.current, modeMenu.unkE, 0, 0, 0, 0);
                }
                else if (modeMenu.unkC & A_BUTTON)
                {
                    m4aSongNumStart(0x21);
                    VS_SEND8(0x202, 0, modeMenu.current, modeMenu.unkE, 0, 0, 0, 0);
                }
                else
                    VS_SEND8(0x200, 0, modeMenu.current, modeMenu.unkE, 0, 0, 0, 0);
            }
            else
            {
                Menu_Draw(&modeMenu);
                if (wait == 0)
                    VS_SEND8(0x200, 0, modeMenu.current, modeMenu.unkE, 0, 0, 0, 0);
                else
                    wait--;
            }
            goto next;
        case 6:
            Bg_LoadScreen(0x10);
            courseMenu.func = Vs_CourseRowsMenuItem;
            courseMenu.itemCount = 2;
            courseMenu.current = 1;
            courseMenu.x = 0x68;
            courseMenu.y = 0x38;
            courseMenu.unkC = 0;
            courseMenu.unkE = 0;
            courseMenu.unk10 = 9;
            wait = 5;
            Sound_PlaySongOrContinue(0xF);
            state = 7;
            break;
        case 7:
            Sprite_ClearLists();
            VS_MENU_LINK(0x300, &courseMenu)
            if (Link_FindCommandExact(1, 0x300, 1) != 0)
            {
                // the parent's B: back to the first menu
                state = 4;
                Menu_Draw(&courseMenu);
                goto next;
            }
            if (Link_FindCommandExact(1, 0x300, 2) != 0)
            {
                // the parent's A: the rows from unkE to unk10 (either order) into unk32
                courseMenu.current = gLinkRecvData[0].unk4;
                courseMenu.unkE = gLinkRecvData[0].unk6;
                courseMenu.unk10 = gLinkRecvData[0].unk8;
                d = courseMenu.unkE - courseMenu.unk10;
                if (d < 0)
                    d = -d;
                lo = courseMenu.unkE > courseMenu.unk10 ? courseMenu.unk10 : courseMenu.unkE;
                for (mask = 1; d > 0; d--)
                    mask = (mask << 1) | 1;
                gGameState.unk32 = mask << lo;
                state = 8;
                Menu_Draw(&courseMenu);
                goto next;
            }
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer > 3)
                    goto next;
                VS_SEND8(0x300, 0, gMain.repeatKeys, 0, 0, 0, 0, 0);
                if (Link_FindCommand(1, 0x300) != 0)
                {
                    courseMenu.current = gLinkRecvData[0].unk4;
                    courseMenu.unkE = gLinkRecvData[0].unk6;
                    courseMenu.unk10 = gLinkRecvData[0].unk8;
                }
                Menu_Draw(&courseMenu);
                goto next;
            }
            got = Link_FindCommand(gLinkPlayers, 0x300);
            courseMenu.unkC = gMain.repeatKeys;
            if (got == gLinkPlayers)
            {
                Menu_RunWrap(&courseMenu, courseMenu.unkC);
                if (courseMenu.unkC & B_BUTTON)
                {
                    m4aSongNumStart(0x22);
                    VS_SEND8(0x301, 0, courseMenu.current, courseMenu.unkE, courseMenu.unk10, 0, 0, 0);
                }
                else if (courseMenu.unkC & A_BUTTON)
                {
                    m4aSongNumStart(0x21);
                    VS_SEND8(0x302, 0, courseMenu.current, courseMenu.unkE, courseMenu.unk10, 0, 0, 0);
                }
                else
                    VS_SEND8(0x300, 0, courseMenu.current, courseMenu.unkE, courseMenu.unk10, 0, 0, 0);
            }
            else
            {
                Menu_Draw(&courseMenu);
                if (wait == 0)
                    VS_SEND8(0x300, 0, courseMenu.current, courseMenu.unkE, courseMenu.unk10, 0, 0, 0);
                else
                    wait--;
            }
            goto next;
        case 8:
        random_course:
            gGameState.world = 12;
            if (gLinkPlayer == 0)
            {
                // the next course (in gVsCourseOrder) whose row of 5 is in unk32
                do
                    n = Vs_NextCourse();
                while (!((gGameState.unk32 >> (n / 5)) & 1));
                gGameState.level = n;
            }
            // fall through
        case 11:
            Bg_LoadScreen(0xF);
            gDispcntCopy = 0x1641;
            settingsMenu.menu.func = (s32 (*)(struct Menu *, s32))Vs_PlayerSettingsItem;
            settingsMenu.menu.itemCount = 2;
            settingsMenu.menu.current = 1;
            settingsMenu.menu.x = 0;
            settingsMenu.menu.y = 4;
            for (i = 0; i < 4; i++)
                settings[i] = ((gLivesSettings.linkLives[i] - 1) << 4) | gLivesSettings.linkSizes[i];
            for (i = 0; i < 4; i++)
            {
                Helirin_Init(&gHelirins[i], i);
                gHelirins[i].angle = 0xC000;
                ((u16 *)&gHelirins[i].x)[1] = 0x60;          // the pixel x
                ((u16 *)&gHelirins[i].y)[1] = 0xC + i * 0x28; // the pixel y
            }
            gCameraX = 0;
            gCameraY = 0;
            gLinkWaitFrames = 0;
            gLinkTimer = 0;
            wait = 5;
            if (gLinkPlayer == 0 && gGameState.unk2C != 0)
            {
                // the level data to send: layers 0 to 2 from layer 0's offset
                const struct WorldEntry *world = &gWorldEntries[gGameState.world];

                lv = &world->info->levels[gGameState.level];
                layer0Size = lv->layers[0].size;
                layer1Start = lv->layers[1].offset - lv->layers[0].offset;
                data = world->data + lv->layers[0].offset;
                total = lv->layers[2].offset - lv->layers[0].offset;
                doneFrames = 0;
                sent = 0;
            }
            Sound_PlaySongOrContinue(0xF);
            state = 12;
            break;
        case 9:
        course_list:
            gMain.ewramCursor = (u8 *)0x02000000;
            Bg_LoadScreen(5);
            gDispcntCopy = 0x1641;
            courseMenu.func = Vs_CourseListMenuItem;
            courseMenu.itemCount = 10;
            courseMenu.x = 0xA8;
            courseMenu.y = 8;
            courseMenu.unkC = 0;
            courseMenu.unk12 = 0;
            courseMenu.current = (u32)gGameState.level / 5 + 1;
            courseMenu.unkE = (u8)((u32)gGameState.level % 5);
            if (courseMenu.current > 7)
                courseMenu.unk10 = 6;
            else
                courseMenu.unk10 = courseMenu.current - 1;
            Sound_PlaySongOrContinue(0xF);
            wait = 5;
            shown = -1;
            state = 10;
            break;
        case 10:
            Sprite_ClearLists();
            VS_MENU_LINK(0x400, &courseMenu)
            if (Link_FindCommandExact(1, 0x400, 1) != 0)
            {
                // the parent's B: back to the first menu
                state = 4;
                Menu_Draw(&courseMenu);
            }
            else if (Link_FindCommandExact(1, 0x400, 2) != 0)
            {
                // the parent's A: the course (row current - 1, column unkE)
                u32 v = gLinkRecvData[0].unk2;

                courseMenu.current = v;
                courseMenu.unkE = (v & 0xFF00) >> 8;
                gGameState.world = 12;
                gGameState.level = courseMenu.unkE + (courseMenu.current - 1) * 5;
                state = 11;
                Menu_Draw(&courseMenu);
            }
            else if (gLinkPlayer != 0)
            {
                if (gLinkPlayer <= 3)
                {
                    VS_SEND8(0x400, gMain.repeatKeys, 0, 0, 0, 0, 0, 0);
                    if (Link_FindCommand(1, 0x400) != 0)
                    {
                        u32 v = gLinkRecvData[0].unk2;

                        courseMenu.current = v;
                        courseMenu.unkE = (v >> 8) & 0xFF;
                    }
                    Menu_Draw(&courseMenu);
                }
            }
            else
            {
                got = Link_FindCommand(gLinkPlayers, 0x400);
                courseMenu.unkC = gMain.repeatKeys;
                if (got == gLinkPlayers)
                {
                    Menu_Run(&courseMenu, courseMenu.unkC);
                    if (courseMenu.unkC & B_BUTTON)
                    {
                        m4aSongNumStart(0x22);
                        VS_SEND8(0x401, courseMenu.current | (courseMenu.unkE << 8), 0, 0, 0, 0, 0, 0);
                    }
                    else if (courseMenu.unkC & A_BUTTON)
                    {
                        m4aSongNumStart(0x21);
                        VS_SEND8(0x402, courseMenu.current | (courseMenu.unkE << 8), 0, 0, 0, 0, 0, 0);
                    }
                    else
                        VS_SEND8(0x400, courseMenu.current | (courseMenu.unkE << 8), 0, 0, 0, 0, 0, 0);
                }
                else
                {
                    Menu_Draw(&courseMenu);
                    if (wait == 0)
                        VS_SEND8(0x400, courseMenu.current | (courseMenu.unkE << 8), 0, 0, 0, 0, 0, 0);
                    else
                        wait--;
                }
            }
            // the selected course's minimap
            n = (courseMenu.current - 1) * 5 + courseMenu.unkE;
            if (shown != n)
            {
                shown = n;
                lv = &gWorldEntries[12].info->levels[shown];
                if (lv->layers[3].size != 0)
                    Main_QueueCopy(gWorldEntries[12].data + lv->layers[3].offset, (void *)0x060174A0, 0x800);
                else if (lv->layers[0].size != 0)
                {
                    Decompress_LZ(gWorldEntries[12].data + lv->layers[0].offset, gMain.ewramCursor,
                                  lv->layers[0].size);
                    Bg_DrawLevelMinimap(gMain.ewramCursor);
                }
                else
                    Main_QueueFill((const void *)0x11111111, (void *)0x060174A0, 0x800);
                sent = 0;
                total = 0x200;
            }
            // the parent sends the minimap to the MultiBoot guests: 8 halfwords a frame, each
            // 8 bytes of it, a bit per pixel (0 for colours 1 and 7, the walls' outline)
            if (gLinkPlayer != 0 || gGameState.unk2C == 0 || wait != 0)
                goto next;
            if (VS_RESEND_ASKED(gGameState.unk2C))
            {
                if (sent <= 0x1F)
                    sent = 0;
                else
                    sent -= 0x20;
            }
            SEND[2] = total;
            if (sent >= total)
                goto next;
            shown = (courseMenu.current - 1) * 5 + courseMenu.unkE;
            {
                const u8 *src = gWorldEntries[12].data + gWorldEntries[12].info->levels[shown].layers[3].offset
                                + sent * 4;

                SEND[3] = sent + 1;
                for (k = 4; k <= 11; k++)
                {
                    u32 bits = 0;
                    s32 j;

                    for (j = 7; j >= 0; j--)
                    {
                        u32 b = *src;

                        bits = (bits << 1) & 0xFFFF;
                        if ((b & 0xF) != 1 && (b & 0xF) != 7)
                            bits |= 1;
                        bits = (bits << 1) & 0xFFFF;
                        if ((b & 0xF0) != 0x10 && (b & 0xF0) != 0x70)
                            bits |= 1;
                        src++;
                    }
                    SEND[k] = bits;
                    sent += 2;
                }
            }
            goto next;
        case 12:
            Sprite_ClearLists();
            VS_MENU_LINK(0x500, &settingsMenu.menu)
            got = Link_FindCommand(gLinkPlayers, 0x500);
            // the other consoles' settings
            for (i = 0; i < 4; i++)
            {
                if ((1 << i) & got & ~gUnk_03000DB4)
                    settings[i] = gLinkRecvData[i].unk2;
            }
            if (Link_FindCommandExact(1, 0x500, 1) != 0)
            {
                // the parent's B: back to the round or course choice
                state = 6;
                if (gGameState.unk2D != 0)
                    state = 9;
                goto next;
            }
            if (Link_FindCommandExact(1, 0x500, 2) != 0)
            {
                // the parent's start: the settings and its course
                for (i = 0; i < 4; i++)
                {
                    gLivesSettings.linkSizes[i] = settings[i] & 0xF;
                    gLivesSettings.linkLives[i] = ((settings[i] & 0x70) >> 4) + 1;
                }
                gGameState.mode = 4;
                gGameState.world = gLinkRecvData[0].unkE;
                gGameState.level = gLinkRecvData[0].unkE >> 8;
                result = 0;
                state = 13;
                goto next;
            }
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer > 3)
                    goto next;
                if (settings[gLinkPlayer] & 0x80)
                    Menu_Draw(&settingsMenu.menu);
                else
                    Menu_RunWrap(&settingsMenu.menu, gMain.newKeys);
                if ((gMain.newKeys & A_BUTTON) && !(settings[gLinkPlayer] & 0x80))
                {
                    m4aSongNumStart(0x21);
                    settings[gLinkPlayer] |= 0x80;
                }
                if ((gMain.newKeys & B_BUTTON) && (settings[gLinkPlayer] & 0x80))
                {
                    m4aSongNumStart(0x22);
                    settings[gLinkPlayer] &= 0xFF7F;
                }
                VS_SEND8(0x500, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
                goto next;
            }
            // the parent: who is ready
            ready = 0;
            for (i = 0; i < 4; i++)
            {
                if ((got & (1 << i)) && (settings[i] & 0x80))
                    ready |= 1 << i;
            }
            if (got == gLinkPlayers)
            {
                if (settings[gLinkPlayer] & 0x80)
                    Menu_Draw(&settingsMenu.menu);
                else
                    Menu_RunWrap(&settingsMenu.menu, gMain.newKeys);
                if (Link_FindCommandExact(gLinkPlayers, 0x500, 1) != 0)
                {
                    VS_SEND8(0x501, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
                    goto next;
                }
                if ((gMain.newKeys & A_BUTTON) && !(settings[gLinkPlayer] & 0x80))
                {
                    m4aSongNumStart(0x21);
                    settings[gLinkPlayer] |= 0x80;
                }
                if (gMain.newKeys & B_BUTTON)
                {
                    m4aSongNumStart(0x22);
                    if (!(settings[gLinkPlayer] & 0x80))
                        VS_SEND8(0x501, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
                    else
                    {
                        settings[gLinkPlayer] &= 0xFF7F;
                        VS_SEND8(0x500, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
                    }
                }
                else if (ready == gLinkPlayers && (gGameState.unk2C == 0 || (s32)doneFrames > 10))
                {
                    // everyone is ready (and has the level data): start
                    VS_SEND8(0x502, settings[gLinkPlayer], total, 0, layer1Start, layer0Size, 0,
                             gGameState.world | (gGameState.level << 8));
                }
                else
                    VS_SEND8(0x500, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
            }
            else
            {
                Menu_Draw(&settingsMenu.menu);
                if (wait == 0)
                    VS_SEND8(0x500, settings[gLinkPlayer], 0, 0, 0, 0, 0, 0);
                else
                    wait--;
            }
            // the level data for the MultiBoot guests: 8 halfwords a frame after the first 4
            if (gGameState.unk2C == 0)
                goto next;
            if (wait == 0)
            {
                if (VS_RESEND_ASKED(gGameState.unk2C))
                {
                    if (sent <= 0x1F)
                        sent = 0;
                    else
                        sent -= 0x20;
                    doneFrames = 0;
                }
                SEND[2] = total;
                if (sent < total)
                {
                    const u16 *src = (const u16 *)(data + sent);

                    SEND[3] = sent + 1;
                    k = 4;
                    do
                    {
                        SEND[k] = *src++;
                        sent += 2;
                        k++;
                    } while (k <= 11 && sent < total);
                }
                else
                    doneFrames++;
            }
            if (ready == gLinkPlayers && sent != total)
                Text_DrawSmall(0x68, 0x4C, gUnk_0801EC1C, (gMain.frameCount & 8) ? 14 : 15);
            Vs_DrawSpinner(sent, total);
            goto next;
        case 13:
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 1);
            Bg_StartWindowCircle(0xA0, 0x78, 0x50);
            gDispcntCopy |= 0x2100;
            m4aSongNumStart(0x2A);
            state = 14;
            break;
        case 14:
            Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
            if (gWindowRadius <= 0)
            {
                m4aMPlayAllStop();
                state = -1;
            }
            break;
        case 15:
            gLinkTimer = 0;
            state = 16;
            // fall through
        case 16:
            if (gLinkTimer++ > 0x78)
            {
                gDispcntCopy |= 0x2100;
                Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
                if (gWindowRadius <= 0)
                {
                    result = -1;
                    state = -1;
                }
            }
            break;
        next:
            Task_RunAll();
            Sprite_BuildOam();
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
extern const u16 gUnk_081339C8[];
extern const u16 gUnk_080C4850[];

/**
 * The linked game's first menu (Vs_RunMenus state 5): item 1 a match of random courses, its
 * rounds unkE (3, 5, 7 or no end: gUnk_080C4844), item 2 the course list. The palette gUnk_081339C8, or the 0x20
 * bytes before it on the current item, is copied to OBJ palette 4 by item 0x101 (which on
 * item 1 also draws three sprites, the middle one tile gUnk_080C4850[unkE]) and to
 * palette 5 by item 0x102; item 1 changes unkE within 0-3 with the keys in unkC. Returns 0.
 */
s32 Vs_MatchMenuItem(struct Menu *p, s32 item)
{
    const u16 *palette;

    if (item == p->current + 0x100)
        palette = gUnk_081339C8 - 0x10;
    else
        palette = gUnk_081339C8;

    switch (item)
    {
    case 0x101:
        Main_QueueCopy(palette, (void *)0x05000080, 0x20);
        if (p->current == 1)
        {
            // One sprite variable per block: each gets its own register, as in the
            // original; word, halfword and byte stores as the original makes them (as
            // Effect_UpdateTimeUpBanner's).
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = 0x40380000;
                    ((u16 *)spr)[2] = 0xB664;
                    *(u8 *)&spr->attr01 = 0x30;
                }
            }
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = 0xC0584000;
                    ((u16 *)spr)[2] = gUnk_080C4850[p->unkE] | 0xB400;
                    *(u8 *)&spr->attr01 = 0x28;
                }
            }
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = 0x40A80000;
                    ((u16 *)spr)[2] = 0xB668;
                    *(u8 *)&spr->attr01 = 0x30;
                }
            }
        }
        break;
    case 0x102:
        Main_QueueCopy(palette, (void *)0x050000A0, 0x20);
        break;
    case 1:
        Menu_StepValueWrap(&p->unkE, 3, p->unkC);
        break;
    case 2:
        break;
    }
    return 0;
}
extern const u8 gTextLeftArrow[];
extern const u8 gTextRightArrow[];

/**
 * The linked game's course rows menu (Vs_RunMenus states 6-7): unkE and unk10 are the first
 * and last row of Challenge courses the random courses come from (gGameState.unk32). 0x100
 * puts the cursor gTextCursor at (0x78, 0x5E), rows 0x20 apart; 0xFFFF draws gTextLeftArrow left and gTextRightArrow right
 * of it (colour 14 or 15, blinking); 0x101 / 0x102 draw a sprite of unkE / unk10 (palette
 * 3 on the current item while bit 3 of the frame counter is set, else 2) and move a row
 * down; items 1 and 2 change unkE / unk10 within 0-9 with the keys in unkC. Returns 0.
 */
s32 Vs_CourseRowsMenuItem(struct Menu *p, s32 item)
{
    u32 palette;
    struct Sprite *spr;

    switch (item)
    {
    case 0x100:
        gTextCursor.x = 0x78;
        gTextCursor.y = 0x5E;
        gTextCursor.dx = 0;
        gTextCursor.dy = 0x20;
        gTextCursor.list = 1;
        break;
    case 0xFFFF:
        palette = (gMain.frameCount & 8) ? 14 : 15;
        Text_DrawSmall((u16)(gTextCursor.x - 0x1C), gTextCursor.y, gTextLeftArrow, palette);
        Text_DrawSmall(gTextCursor.x + 0x14, gTextCursor.y, gTextRightArrow, palette);
        break;
    case 0x101:
        palette = ((gMain.frameCount & 8) && item == p->current + 0x100) ? 3 : 2;
        spr = Sprite_Alloc(9);
        if (spr != NULL)
        {
            spr->attr01 = (((gTextCursor.x - 0x10) << 16) & 0x1FF0000) | 0x80004000;
            // the cast keeps agbcc from reassociating 0x800 to the end, as the original
            spr->attr2 = (u16)((palette << 12) | 0x800) | (p->unkE * 8 + 0x120);
            *(u8 *)&spr->attr01 = 0x5A;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 0x102:
        palette = ((gMain.frameCount & 8) && item == p->current + 0x100) ? 3 : 2;
        spr = Sprite_Alloc(9);
        if (spr != NULL)
        {
            spr->attr01 = (((gTextCursor.x - 0x10) << 16) & 0x1FF0000) | 0x80004000;
            spr->attr2 = (u16)((palette << 12) | 0x800) | (p->unk10 * 8 + 0x120);
            *(u8 *)&spr->attr01 = 0x7A;
        }
        gTextCursor.y += gTextCursor.dy;
        break;
    case 1:
        Menu_StepValueWrap(&p->unkE, 9, p->unkC);
        break;
    case 2:
        Menu_StepValueWrap(&p->unk10, 9, p->unkC);
        break;
    }
    return 0;
}
extern const u8 gUnk_0801EC2C[]; // "P" (before a player's number)

/**
 * The course list of the linked game (Vs_RunMenus states 9-10): 0x100 draws the standings:
 * a header (sprites, gGameState.unk30 + 1), then for places 0-3 (gGameState.unk3C) each
 * present player's "P" and number, its gGameState.unk34 and a mark (colour 15 for this
 * console's player while bit 3 of the frame counter is set, else 14); it also scrolls the
 * course list so that the current item stays within the 4 rows from unk10 (unk12: a scroll
 * offset of -0x20 or 0x20 that moves back to 0 by 4 a frame, which sets where the rows
 * start). 0x101-0x10B draw row *item* - 0x101 when it is shown: a sprite, then five cells
 * (column unkE of the current row blinking in palette 5, else 4). Items 1-11 change unkE
 * within 0-4 with the keys in unkC. Returns 0.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Vs_CourseListMenuItem(struct Menu *p, s32 item)
{
    struct Sprite *spr;
    s32 row, last, palette, x;
    s32 place, i, j;

    switch (item)
    {
    case 0x100:
        if (p->current - 1 < p->unk10)
        {
            p->unk10--;
            p->unk12 = -0x20;
        }
        if (p->current - 1 >= p->unk10 + 4)
        {
            p->unk10++;
            p->unk12 = 0x20;
        }
        spr = Sprite_Alloc(2);
        if (spr != NULL)
        {
            spr->attr01 = (p->x << 16) | 0xC0000000;
            spr->attr2 = 0xC3A5;
            *(u8 *)&spr->attr01 = p->y;
        }
        gTextCursor.x = p->x;
        gTextCursor.y = p->y + 0x42;
        gTextCursor.dx = 8;
        gTextCursor.dy = 9;
        gTextCursor.list = 1;
        gTextCursor.palette = 14;
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40004000;
            spr->attr2 = 0xE3FB;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        Text_DrawSmallNumber(gTextCursor.x + 0x20, gTextCursor.y, gGameState.unk30 + 1, 14);
        gTextCursor.y += gTextCursor.dy;
        for (place = 0; place < 4; place++)
        {
            for (i = 0; i < 4; i++)
            {
                if (!((gLinkPlayers >> i) & 1) || gGameState.unk3C[i] != place)
                    continue;
                if ((gMain.frameCount & 8) && i == gLinkPlayer)
                    gTextCursor.palette = 15;
                else
                    gTextCursor.palette = 14;
                gTextCursor.x = p->x;
                Text_DrawSmall(gTextCursor.x, gTextCursor.y, gUnk_0801EC2C, gTextCursor.palette);
                Text_DrawSmallNumberAligned(gTextCursor.x + 8, gTextCursor.y, i + 1, 1,
                                            gTextCursor.palette);
                gTextCursor.y += gTextCursor.dy;
                Text_DrawSmallNumberAligned(gTextCursor.x + 8, gTextCursor.y, gGameState.unk34[i], 3,
                                            gTextCursor.palette);
                spr = Sprite_Alloc(gTextCursor.list);
                if (spr != NULL)
                {
                    spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40004000;
                    spr->attr2 = (gTextCursor.palette << 12) | ((gTextCursor.list >> 2) << 10) | 0xA0;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                gTextCursor.y += gTextCursor.dy;
            }
        }
        gTextCursor.x = 4;
        // where the rows of items 0x101.. start
        if ((s16)p->unk12 > 0)
        {
            p->unk12 -= 4;
            if (p->unk12 == 0)
                gTextCursor.y = 0x28;
            else
                gTextCursor.y = p->unk12 + 8;
        }
        else if ((s16)p->unk12 < 0)
        {
            p->unk12 += 4;
            gTextCursor.y = p->unk12 + 0x28;
        }
        else
            gTextCursor.y = 0x28;
        break;
    case 0x101 ... 0x10B:
        // the shown rows: unk10 to unk10 + 3, and the one scrolling in
        row = item - 0x101;
        if ((s16)p->unk12 > 0)
        {
            if (p->unk10 > row + 1)
                return 0;
            last = p->unk10 + 4;
        }
        else if ((s16)p->unk12 < 0)
        {
            if (p->unk10 > row)
                return 0;
            last = p->unk10 + 5;
        }
        else
        {
            if (p->unk10 > row)
                return 0;
            last = p->unk10 + 4;
        }
        if (row >= last)
            return 0;
        spr = Sprite_Alloc(9);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (row * 8 + 0x120) | 0x4800;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        for (j = 0, x = 0x28; j < 5; j++, x += 0x18)
        {
            if (p->unkE == j && p->current - 1 == row)
                palette = 4 + !((gMain.frameCount >> 3) & 1);
            else
                palette = 4;
            spr = Sprite_Alloc(9);
            if (spr != NULL)
            {
                spr->attr01 = (((gTextCursor.x + x - 8) << 16) & 0x1FF0000) | 0x80000000;
                spr->attr2 = (palette << 12) | 0x800 | (j * 16 + 0x178);
                *(u8 *)&spr->attr01 = gTextCursor.y - 8;
            }
        }
        gTextCursor.y += 0x20;
        break;
    case 1 ... 11:
        Menu_StepValueWrap(&p->unkE, 4, p->unkC);
        break;
    }
    return 0;
}
void Vs_DrawNumberRight(s32 x, s32 y, u16 value, s32 palette);
extern const u16 gUnk_080C4858[];

/**
 * A menu function of the linked game (Vs_RunMenus): each player's settings are a u16 at
 * unkC + 2 * n (bits 0-3 its Helirin's size, bits 4-6 its lives - 1 (0-2), bit 7 ready; Vs_RunMenus
 * hands them to gLivesSettings). Items 1 and 2 change this console's player's size (0-14,
 * reloading the Helirin's tiles with Helirin_SetSize) and lives with the keys; 0x100 puts
 * the cursor gTextCursor at y 0, 0x1FF draws a 0x28-pixel row per player: OBJ palette 5 + n
 * from gUnk_08133948 (palette 9 + n for an absent player or a ready one), and for a present
 * player who answered (Link_FindCommand(.., 0x500)) its Helirin at the chosen size, a mark
 * per life and gGameState.unk34[n], with the arrows of the current item for this console's
 * player. Returns 0.
 */
s32 Vs_PlayerSettingsItem(struct VsSettingsMenu *p, s32 item)
{
    u16 value, count;
    s32 ready;
    struct Helirin *h;
    s32 i, j, x, y;
    u32 flag, palette;

    switch (item)
    {
    case 0x100:
        gTextCursor.y = 0;
        gTextCursor.dx = 8;
        gTextCursor.dy = 8;
        gTextCursor.list = 4;
        gTextCursor.palette = 14;
        break;
    case 0x101:
    case 0x102:
        break;
    case 1:
        value = p->settings[gLinkPlayer] & 0xF;
        if (Menu_StepValueReversed(&value, 14, gMain.repeatKeys))
            Helirin_SetSize(&gHelirins[gLinkPlayer]);
        p->settings[gLinkPlayer] = (p->settings[gLinkPlayer] & 0xF0) | value;
        break;
    case 2:
        count = (p->settings[gLinkPlayer] & 0x70) >> 4;
        Menu_StepValue(&count, 2, gMain.repeatKeys);
        p->settings[gLinkPlayer] = (p->settings[gLinkPlayer] & 0x8F) | (count << 4);
        break;
    case 0x1FF:
        ready = Link_FindCommand(gLinkPlayers, 0x500);
        for (i = 0; i < 4; i++)
        {
            h = &gHelirins[i];
            x = 0x10;
            y = gTextCursor.y;
            flag = p->settings[i] & 0x80;
            value = p->settings[i] & 0xF;
            count = (p->settings[i] & 0x70) >> 4;
            if (!((gLinkPlayers >> i) & 1))
            {
                Main_QueueCopy(gUnk_08133948 + (i + 9) * 0x20, (void *)(0x05000000 + (i + 5) * 0x20), 0x20);
                gTextCursor.y += 0x28;
                continue;
            }
            if (!((ready >> i) & 1))
            {
                Main_QueueCopy(gUnk_08133948 + (i + 5) * 0x20, (void *)(0x05000000 + (i + 5) * 0x20), 0x20);
                gTextCursor.y += 0x28;
                continue;
            }
            if (flag == 0)
                Main_QueueCopy(gUnk_08133948 + (i + 5) * 0x20, (void *)(0x05000000 + (i + 5) * 0x20), 0x20);
            else
                Main_QueueCopy(gUnk_08133948 + (i + 9) * 0x20, (void *)(0x05000000 + (i + 5) * 0x20), 0x20);
            if (i == gLinkPlayer && (gMain.frameCount & 8))
                palette = 0x5000;
            else
                palette = h->palette;
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                    ((u16 *)spr)[2] = palette | 0x66C;
                    *(u8 *)&spr->attr01 = gTextCursor.y + 0x10;
                }
            }
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = (((x + 0x10) << 16) & 0x1FF0000) | 0x40000000;
                    // the cast keeps agbcc from reassociating 0x400 to the end (the
                    // original ORs it in first, as Vs_CourseRowsMenuItem)
                    ((u16 *)spr)[2] = (u16)(palette | 0x400) | gUnk_080C4858[i];
                    *(u8 *)&spr->attr01 = gTextCursor.y + 0x10;
                }
            }
            x += 0x28;
            if ((s8)h->size != value)
            {
                h->size = value;
                Helirin_SetSize(h);
            }
            if (i == gLinkPlayer && !(p->settings[i] & 0x80) && p->unk1 == 1)
            {
                if (gMain.frameCount & 8)
                    gTextCursor.palette = 14;
                else
                    gTextCursor.palette = 15;
                Text_DrawSmall((u16)x, (u8)(y + 8), gTextLeftArrow, gTextCursor.palette);
                Text_DrawSmall((u16)(x + 0x48), (u8)(y + 8), gTextRightArrow, gTextCursor.palette);
            }
            Helirin_Draw(h);
            y += 0x12;
            if (i == gLinkPlayer && !(p->settings[i] & 0x80) && p->unk1 == 2)
            {
                if (gMain.frameCount & 8)
                    gTextCursor.palette = 14;
                else
                    gTextCursor.palette = 15;
                Text_DrawSmall((u16)x, (u8)(y + 4), gTextLeftArrow, gTextCursor.palette);
                Text_DrawSmall((u16)(x + 0x48), (u8)(y + 4), gTextRightArrow, gTextCursor.palette);
            }
            palette = 7;
            x += 0xC;
            for (j = 0; j <= count; j++)
            {
                struct Sprite *spr = Sprite_Alloc(5);

                if (spr != NULL)
                {
                    spr->attr01 = (((x + j * 0x14) << 16) & 0x1FF0000) | 0x40000000;
                    ((u16 *)spr)[2] = (palette << 12) | 0x6A0;
                    *(u8 *)&spr->attr01 = y;
                }
            }
            Vs_DrawNumberRight(x + 0x34, gTextCursor.y + 0x10, gGameState.unk34[i], 14);
            gTextCursor.y += 0x28;
        }
        break;
    }
    return 0;
}
void Vs_DrawNumber(s32 x, s32 y, u16 value, s32 palette);
extern const u16 gUnk_080C4868[]; // the rank sprites' first tiles (two 32x16 sprites each)
extern const u16 gUnk_080C4860[4]; // the players' marks (sprite tiles)

// The menus of Vs_RunResults: a struct Menu, then the per-player results of the level
struct VsResultsMenu {
    /*0x00*/ struct Menu menu;
    /*0x18*/ u8 unk18[4];
    /*0x1C*/ const u8 *ranks;    // per player: its place, 0-3
    /*0x20*/ const u32 *times;   // per player: its time in frames (Text_DrawDigitTime)
    /*0x24*/ const u16 *numbers; // per player: a number drawn after its time (Vs_DrawNumber)
    /*0x28*/ const u16 *totals;  // per player: the number Vs_ResultsMenuItem2 draws (Vs_DrawNumberRight)
};
s32 Vs_ResultsMenuItem(struct VsResultsMenu *p, s32 item);
s32 Vs_ResultsMenuItem2(struct VsResultsMenu *p, s32 item);
extern const struct TileCopy gUnk_0801EC30[]; // Vs_RunResults's OBJ tiles
extern const u16 gUnk_08133748[];             // its OBJ palettes (0x1C0 bytes)
extern const u8 gUnk_080C4848[];              // the points of a place: [place + 4 - players]

// The link commands of Vs_RunResults (gLinkSendData[0]; Link_FindCommand matches the high byte)
#define VS_SEND(a, b, c, d, e, f, g, h) \
    do                                  \
    {                                   \
        gLinkSendData[0] = (a);         \
        gLinkSendData[1] = (b);         \
        gLinkSendData[2] = (c);         \
        gLinkSendData[3] = (d);         \
        gLinkSendData[4] = (e);         \
        gLinkSendData[5] = (f);         \
        gLinkSendData[6] = (g);         \
        gLinkSendData[7] = (h);         \
    } while (0)
// a player's time and bump count (command 0x900: from every player; 0x901: the parent's
// places as well, a nibble per player)
#define VS_SEND_TIME(cmd, keys, places)                                                     \
    VS_SEND(cmd, 0, keys, gGameState.timer >> 16, gGameState.timer, gGameState.bumpCount, \
            places, 0)
#define VS_PLACES                                                                          \
    ((gGameState.unk3C[3] << 12) | (gGameState.unk3C[2] << 8) | (gGameState.unk3C[1] << 4) \
     | gGameState.unk3C[0])

/**
 * The linked game's results after a level (Play_MainState), with its own frame loop over 8
 * states, through the link (gLinkSendData / gLinkRecvData; this console's player is
 * gLinkPlayer, the parent is player 0):
 * 0: fades the music out, loads the screen's tiles and palettes, starts the tasks of the
 *    banner animation and the result song, and sends its time (command 0x900).
 * 1: the parent collects every player's time and bump count (Link_FindCommand), ranks them
 *    (gGameState.unk3C: the number of faster players; 4 for a time-up) and sends the places
 *    (0x901); the others take the places from the parent's 0x901 and go to 2.
 * 2: the times again (the menu Vs_ResultsMenuItem shows places, times and bump counts); the
 *    parent, on A or B, adds the places' points to gGameState.unk34 (up to 999; not when no
 *    player finished) and sends them with the round counter unk30 (0xA00). On 0xA00 every
 *    console takes the totals, ranks the players by them, and goes on to the second menu
 *    (Vs_ResultsMenuItem2): 4 with the final banner once round unk30 + 1 reaches unk2E, else
 *    3 with two choices.
 * 3, 4: the parent runs the menu (3) or waits for A/B (4) and sends the choice: 0x201 play
 *    on, 0x102 quit; the others follow its current item (0xA01 / 0xA02). On the parent's
 *    0x200 the round counter goes up (unless no player finished) and the result is 1, on
 *    0x100 it is 2; both close the window circle in 5.
 * 5: shrinks the window circle, then leaves (state -1).
 * 6, 7: a link error (Vs_CheckMenuLink -1): after 0x78 frames shrinks the circle at the
 *    centre and leaves with -1.
 * Returns the result: 1 another round, 2 quit, -1 a link error.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Vs_RunResults(void)
{
    struct VsResultsMenu menu;
    u32 times[4];
    u16 numbers[4];
    s32 state = 0;
    s32 result = 0;
    s32 noneFinished;
    u32 got;
    s32 i, j, n;
    const struct TileCopy *e;

    // (the original copies the four times from the ROM constant gUnk_0801ECA8)
    for (i = 0; i < 4; i++)
        times[i] = 216000; // 60 minutes: no time
    memset(numbers, 0, sizeof(numbers));
    do
    {
        switch (state)
        {
        case 0:
            m4aMPlayFadeOut(&gMPlayInfo_BGM, 2);
            Sprite_ClearLists();
            Sprite_BuildOam();
            Main_EndFrame();
            gWinoutCopy = 0x11;
            for (e = gUnk_0801EC30; e->src != NULL; e++)
                Main_QueueCopy(e->src, (u8 *)0x06010000 + e->tile * 32, e->tiles * 32);
            Main_QueueCopy(gUnk_08133748, (void *)0x05000200, 0x1C0);
            gGameState.helirin->regenTimer = 0;
            gGameState.helirin->lives = gGameState.helirin->maxLives;
            menu.menu.func = (s32 (*)(struct Menu *, s32))Vs_ResultsMenuItem;
            menu.menu.itemCount = 0;
            menu.menu.current = 1;
            menu.menu.x = 0;
            menu.menu.y = 0x28;
            menu.menu.unkC = 0;
            menu.ranks = gGameState.unk3C;
            menu.times = times;
            menu.numbers = numbers;
            menu.totals = gGameState.unk34;
            Task_Init();
            {
                struct CopyAnim *task = (struct CopyAnim *)Task_Create(Effect_AnimateCopies, sizeof(*task));

                if (task != NULL)
                {
                    task->anim.timer = 1;
                    task->anim.next = gUnk_080C6270;
                    task->anim.value = gUnk_080C6270[1];
                    task->copies = gUnk_080C6284;
                }
            }
            {
                struct SongAfterPlayer *task = (struct SongAfterPlayer *)Task_Create(
                    (void (*)(struct Task *))Sound_UpdateSongAfterPlayer, sizeof(*task));

                if (task != NULL)
                {
                    task->player = &gMPlayInfo_BGM;
                    task->song = 0x14;
                    task->delay = 0x3C;
                }
            }
            VS_SEND_TIME(0x900, 0, 0);
            state = 1;
            // fall through
        case 1:
            Sprite_ClearLists();
            switch (Vs_CheckMenuLink(0x900))
            {
            case -1:
                state = 6;
                // fall through
            case 1:
                Menu_Draw(&menu.menu);
                // fall through
            default:
                goto next;
            case 0:
                break;
            }
            if (Link_FindCommandExact(1, 0x900, 1) != 0)
            {
                // the parent's places
                u32 places = gLinkRecvData[0].unkC;

                gGameState.unk3C[0] = places & 0xF;
                gGameState.unk3C[1] = (places >> 4) & 0xF;
                gGameState.unk3C[2] = (places >> 8) & 0xF;
                gGameState.unk3C[3] = (places >> 12) & 0xF;
                state = 2;
                goto next;
            }
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer <= 3)
                    VS_SEND_TIME(0x900, gMain.newKeys, 0);
                goto next;
            }
            got = Link_FindCommand(gLinkPlayers, 0x900);
            if (got != gLinkPlayers)
                goto next;
            for (i = 0; i < 4; i++)
            {
                if ((got >> i) & 1)
                {
                    times[i] = (gLinkRecvData[i].unk6 << 16) | gLinkRecvData[i].unk8;
                    numbers[i] = gLinkRecvData[i].unkA;
                }
            }
            for (i = 3; i >= 0; i--)
                gGameState.unk3C[i] = 0;
            for (i = 0; i < 4; i++)
            {
                if (!((got >> i) & 1))
                    continue;
                for (j = 0; j < 4; j++)
                {
                    if (i != j && ((got >> j) & 1) && times[i] < times[j])
                        gGameState.unk3C[j]++;
                }
            }
            n = 0;
            for (i = 0; i < 4; i++)
            {
                if ((got >> i) & 1)
                {
                    if (gHelirins[i].flags & 0x400) // time up
                        gGameState.unk3C[i] = 4;
                    else
                        n++;
                }
            }
            noneFinished = (n == 0);
            VS_SEND_TIME(0x901, menu.menu.current, VS_PLACES);
            goto next;
        case 2:
            Sprite_ClearLists();
            switch (Vs_CheckMenuLink(0x900))
            {
            case -1:
                state = 6;
                // fall through
            case 1:
                Menu_Draw(&menu.menu);
                // fall through
            default:
                goto next;
            case 0:
                break;
            }
            got = Link_FindCommand(gLinkPlayers, 0x900);
            for (i = 0; i < 4; i++)
            {
                if ((got >> i) & 1)
                {
                    times[i] = (gLinkRecvData[i].unk6 << 16) | gLinkRecvData[i].unk8;
                    numbers[i] = gLinkRecvData[i].unkA;
                }
            }
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer > 3)
                    goto totals;
                VS_SEND_TIME(0x900, gMain.newKeys, 0);
            }
            else
            {
                got = Link_FindCommand(gLinkPlayers, 0x900);
                menu.menu.unkC = gMain.newKeys;
                if (got == gLinkPlayers)
                {
                    if (menu.menu.unkC & (A_BUTTON | B_BUTTON))
                    {
                        m4aSongNumStart(0x21);
                        if (!noneFinished)
                        {
                            for (i = 0; i < 4; i++)
                            {
                                if ((gLinkPlayers >> i) & 1)
                                {
                                    gGameState.unk34[i] += gUnk_080C4848[gGameState.unk3C[i] + 4 - gGameState.playerCount];
                                    if (gGameState.unk34[i] > 999)
                                        gGameState.unk34[i] = 999;
                                }
                            }
                        }
                        VS_SEND(0xA00, 0, gGameState.unk34[0], gGameState.unk34[1], gGameState.unk34[2],
                                gGameState.unk34[3], gGameState.unk30, noneFinished);
                    }
                    else
                        VS_SEND_TIME(0x901, menu.menu.current, VS_PLACES);
                }
            }
            Menu_Draw(&menu.menu);
        totals:
            if (!(Link_FindCommand(gLinkPlayers, 0xA00) & 1))
                goto next;
            // the parent's totals
            gGameState.unk34[0] = gLinkRecvData[0].unk4;
            gGameState.unk34[1] = gLinkRecvData[0].unk6;
            gGameState.unk34[2] = gLinkRecvData[0].unk8;
            gGameState.unk34[3] = gLinkRecvData[0].unkA;
            gGameState.unk30 = gLinkRecvData[0].unkC;
            noneFinished = gLinkRecvData[0].unkE;
            got = gLinkPlayers;
            for (i = 3; i >= 0; i--)
                gGameState.unk3C[i] = 0;
            for (i = 0; i < 4; i++)
            {
                if (!((got >> i) & 1))
                    continue;
                for (j = 0; j < 4; j++)
                {
                    if (i != j && ((got >> j) & 1) && gGameState.unk34[i] > gGameState.unk34[j])
                        gGameState.unk3C[j]++;
                }
            }
            if (gGameState.unk2E != 0 && gGameState.unk2E <= gGameState.unk30 + 1)
            {
                menu.menu.func = (s32 (*)(struct Menu *, s32))Vs_ResultsMenuItem2;
                menu.menu.itemCount = 0;
                menu.menu.current = 0;
                Task_Create(Effect_UpdateLinkResultBanner, 0x10);
                state = 4;
            }
            else
            {
                menu.menu.func = (s32 (*)(struct Menu *, s32))Vs_ResultsMenuItem2;
                menu.menu.itemCount = 2;
                menu.menu.current = 1;
                state = 3;
            }
            goto next;
        case 3:
        case 4:
            Sprite_ClearLists();
            switch (Vs_CheckMenuLink(0xA00))
            {
            case -1:
                state = 6;
                // fall through
            case 1:
                Menu_Draw(&menu.menu);
                // fall through
            default:
                goto next;
            case 0:
                break;
            }
            Link_FindCommand(gLinkPlayers, 0xA00);
            if (gLinkPlayer != 0)
            {
                if (gLinkPlayer <= 3)
                {
                    VS_SEND(0xA01, 0, gMain.newKeys, 0, 0, 0, 0, 0);
                    if (Link_FindCommandExact(gLinkPlayers, 0xA00, 2) != 0)
                        menu.menu.current = gLinkRecvData[0].unk4;
                    Menu_Draw(&menu.menu);
                }
            }
            else
            {
                got = Link_FindCommand(gLinkPlayers, 0xA00);
                menu.menu.unkC = gMain.newKeys;
                if (got != gLinkPlayers)
                {
                    Menu_Draw(&menu.menu);
                    VS_SEND(0xA01, 0, menu.menu.current, 0, 0, 0, 0, 0);
                }
                else if (state == 3)
                {
                    switch (Menu_RunWrap(&menu.menu, menu.menu.unkC))
                    {
                    case 0:
                        VS_SEND(0xA02, 0, menu.menu.current, 0, 0, 0, 0, 0);
                        break;
                    case 1: // play on
                        VS_SEND(0x201, 0, 0, 0, 0, 0, 0, 0);
                        break;
                    case 2: // quit
                        VS_SEND(0x102, 0, 0, 0, 0, 0, 0, 0);
                        break;
                    }
                }
                else
                {
                    Menu_Draw(&menu.menu);
                    if (menu.menu.unkC & (A_BUTTON | B_BUTTON))
                        VS_SEND(0x102, 0, 0, 0, 0, 0, 0, 0);
                    else
                        VS_SEND(0xA02, 0, 0, 0, 0, 0, 0, 0);
                }
            }
            if (Link_FindCommand(1, 0x200) != 0)
            {
                if (!noneFinished && gGameState.unk30 <= 997)
                    gGameState.unk30++;
                result = 1;
                state = 5;
            }
            else if (Link_FindCommand(1, 0x100) != 0)
            {
                result = 2;
                state = 5;
            }
            goto next;
        case 5:
            if (gWindowRadius-- <= 0)
            {
                gDispcntCopy = (gDispcntCopy & 0xE0FF) | 0x100;
                state = -1;
            }
            Bg_DrawWindowCircle(gWindowRadius, gWindowX, gWindowY);
            Sprite_ClearLists();
        next:
            Task_RunAll();
            Sprite_BuildOam();
            break;
        case 6:
            gLinkTimer = 0;
            state = 7;
            // fall through
        case 7:
            if (gLinkTimer++ > 0x78)
            {
                gDispcntCopy |= 0x2100;
                Bg_DrawWindowCircle(gWindowRadius - 8, 0x78, 0x50);
                if (gWindowRadius <= 0)
                {
                    result = -1;
                    state = -1;
                }
            }
            break;
        }
        Main_EndFrame();
    } while (state != -1);
    return result;
}
/**
 * Vs_RunResults's first results menu function: only item 0x100 does anything. It puts
 * gTextCursor at the menu's position (dy 0x16, list 1, palette 14), draws the header (three
 * sprites) with gGameState.unk30 + 1, then one row per present player (gLinkPlayers) in the
 * order of their places 0-4: the place's two sprites (gUnk_080C4868, places 0-3), the
 * player's mark (its Helirin's palette, or palette 5 for this console's player while bit 3
 * of the frame counter is set), its time (Text_DrawDigitTime) and number (Vs_DrawNumber) in
 * colour 14 (15 for this console's blinking player). Returns 0.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Vs_ResultsMenuItem(struct VsResultsMenu *p, s32 item)
{
    struct Sprite *spr;
    const u8 *ranks;
    const u32 *times;
    const u16 *numbers;
    s32 r, i, x;
    u32 palette, color;

    if (item != 0x100)
        return 0;
    gTextCursor.x = p->menu.x;
    gTextCursor.y = p->menu.y;
    gTextCursor.dx = 8;
    gTextCursor.dy = 0x16;
    gTextCursor.list = 1;
    gTextCursor.palette = 14;
    spr = Sprite_Alloc(3);
    if (spr != NULL)
    {
        spr->attr01 = 0xC0484000;
        spr->attr2 = 0xD3A5;
        *(u8 *)&spr->attr01 = 0;
    }
    spr = Sprite_Alloc(3);
    if (spr != NULL)
    {
        spr->attr01 = 0x80880000;
        spr->attr2 = 0xD3C5;
        *(u8 *)&spr->attr01 = 0;
    }
    spr = Sprite_Alloc(3);
    if (spr != NULL)
    {
        spr->attr01 = 0x40B84000;
        spr->attr2 = 0xE3FB;
        *(u8 *)&spr->attr01 = 0x18;
    }
    Text_DrawSmallNumber(0xD8, 0x18, gGameState.unk30 + 1, 14);
    ranks = p->ranks;
    times = p->times;
    numbers = p->numbers;
    for (r = 0; r < 5; r++)
    {
        for (i = 0; i < 4; i++)
        {
            if (!((gLinkPlayers >> i) & 1) || ranks[i] != r)
                continue;
            x = 0x10;
            if (r < 4)
            {
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40100000;
                    spr->attr2 = gUnk_080C4868[ranks[i]] | 0xB000;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40200000;
                    spr->attr2 = (gUnk_080C4868[ranks[i]] + 4) | 0xB000;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
            }
            x += 0x30;
            if ((gMain.frameCount & 8) && i == gLinkPlayer)
                palette = 0x5000;
            else
                palette = gHelirins[i].palette;
            spr = Sprite_Alloc(3);
            if (spr != NULL)
            {
                spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                spr->attr2 = palette | 0x1E4;
                *(u8 *)&spr->attr01 = gTextCursor.y;
            }
            spr = Sprite_Alloc(3);
            if (spr != NULL)
            {
                spr->attr01 = (((x + 0x10) << 16) & 0x1FF0000) | 0x40000000;
                spr->attr2 = palette | gUnk_080C4860[i];
                *(u8 *)&spr->attr01 = gTextCursor.y;
            }
            x += 0x30;
            if ((gMain.frameCount & 8) && i == gLinkPlayer)
                color = 15;
            else
                color = 14;
            Text_DrawDigitTime(3, x, gTextCursor.y, times[i], color);
            x += 0x50;
            spr = Sprite_Alloc(3);
            if (spr != NULL)
            {
                spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                spr->attr2 = (color << 12) | 0x1E0;
                *(u8 *)&spr->attr01 = gTextCursor.y;
            }
            Vs_DrawNumber(x + 0x10, gTextCursor.y, numbers[i], color);
            gTextCursor.y += gTextCursor.dy;
        }
    }
    return 0;
}
/**
 * The twin of Vs_ResultsMenuItem, Vs_RunResults's second results menu function. The
 * palette of gTextCursor is 14 on the current item while bit 3 of the frame counter is
 * set, else 15. Items 1 and 2 return the item (with song 0x21) when bit 0 of unkC (A) is
 * set. 0x100 draws the header and one row per present player in the order of its place
 * 0-3: the place's two sprites, the player's mark and its number totals[n]
 * (Vs_DrawNumberRight; colour 15 for this console's blinking player, else 14). 0x101 and
 * 0x102 draw two sprites each (two choices at (0x68, 0x80), 0x10 apart). Returns 0 for any
 * other case.
 * Functional C, not byte-identical, never executed (docs/audit.md, final C pass).
 */
s32 Vs_ResultsMenuItem2(struct VsResultsMenu *p, s32 item)
{
    struct Sprite *spr;
    const u8 *ranks;
    const u16 *totals;
    s32 r, i, x;
    u32 palette, color;

    if ((gMain.frameCount & 8) && item == p->menu.current + 0x100)
        gTextCursor.palette = 14;
    else
        gTextCursor.palette = 15;

    switch (item)
    {
    case 1:
    case 2:
        if (p->menu.unkC & A_BUTTON)
        {
            m4aSongNumStart(0x21);
            return item;
        }
        break;
    case 0x100:
        gTextCursor.x = p->menu.x;
        gTextCursor.y = p->menu.y;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x16;
        gTextCursor.list = 1;
        gTextCursor.palette = 14;
        ranks = p->ranks;
        totals = p->totals;
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = 0xC0484000;
            spr->attr2 = 0xD3A5;
            *(u8 *)&spr->attr01 = 0;
        }
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = 0x80880000;
            spr->attr2 = 0xD3C5;
            *(u8 *)&spr->attr01 = 0;
        }
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = 0x40B84000;
            spr->attr2 = 0xE3FB;
            *(u8 *)&spr->attr01 = 0x18;
        }
        Text_DrawSmallNumber(0xD8, 0x18, gGameState.unk30 + 1, 14);
        for (r = 0; r < 4; r++)
        {
            for (i = 0; i < 4; i++)
            {
                if (!((gLinkPlayers >> i) & 1) || ranks[i] != r)
                    continue;
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40100000;
                    spr->attr2 = gUnk_080C4868[ranks[i]] | 0xB000;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = 0x40200000;
                    spr->attr2 = (gUnk_080C4868[ranks[i]] + 4) | 0xB000;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                x = 0x40;
                if ((gMain.frameCount & 8) && i == gLinkPlayer)
                    palette = 0x5000;
                else
                    palette = gHelirins[i].palette;
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = ((x << 16) & 0x1FF0000) | 0x40000000;
                    spr->attr2 = palette | 0x1E4;
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                spr = Sprite_Alloc(3);
                if (spr != NULL)
                {
                    spr->attr01 = (((x + 0x10) << 16) & 0x1FF0000) | 0x40000000;
                    spr->attr2 = palette | gUnk_080C4860[i];
                    *(u8 *)&spr->attr01 = gTextCursor.y;
                }
                x += 0x30;
                if ((gMain.frameCount & 8) && i == gLinkPlayer)
                    color = 15;
                else
                    color = 14;
                Vs_DrawNumberRight(x - 0x20, gTextCursor.y, totals[i], color);
                gTextCursor.y += gTextCursor.dy;
            }
        }
        break;
    case 0x101:
        gTextCursor.x = 0x68;
        gTextCursor.y = 0x80;
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x80004000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x218;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x20) << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x220;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        gTextCursor.y += 0x10;
        break;
    case 0x102:
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = (gTextCursor.x << 16) | 0x40000000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x224;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        spr = Sprite_Alloc(3);
        if (spr != NULL)
        {
            spr->attr01 = ((gTextCursor.x + 0x10) << 16) | 0x8000;
            spr->attr2 = (gTextCursor.palette << 12) | 0x228;
            *(u8 *)&spr->attr01 = gTextCursor.y;
        }
        break;
    }
    return 0;
}
/**
 * Vs_DrawNumber right-aligned in five cells (a leading zero leaves its cell empty), followed
 * by a wide (16x8) sprite with tile 0x1D8 in the next cell.
 */
void Vs_DrawNumberRight(s32 x, s32 y, u16 value, s32 palette)
{
    u32 n = value;
    u32 div = 10000;
    s32 started = FALSE;
    s32 digit;
    struct Sprite *e;
    struct Sprite *last; // its own variable, as in Vs_DrawNumber

    do
    {
        for (digit = 0; n >= div; n -= div)
            digit++;
        if (digit != 0 || started)
        {
            started = TRUE;
            e = Sprite_Alloc(3);
            if (e != NULL)
            {
                e->attr01 = ((x << 16) & 0x1FF0000) | 0x8000;
                e->attr2 = (palette << 12) | (0x40 + digit * 2);
                *(u8 *)&e->attr01 = y;
            }
        }
        x += 8;
        div /= 10;
    } while (div > 9);
    last = Sprite_Alloc(3);
    if (last != NULL)
    {
        last->attr01 = ((x << 16) & 0x1FF0000) | 0x8000;
        last->attr2 = (palette << 12) | (0x40 + n * 2);
        *(u8 *)&last->attr01 = y;
    }
    x += 8;
    last = Sprite_Alloc(3);
    if (last != NULL)
    {
        last->attr01 = ((x << 16) & 0x1FF0000) | 0x80004000;
        last->attr2 = (palette << 12) | 0x1D8;
        *(u8 *)&last->attr01 = y;
    }
}
/**
 * Draws *value* in decimal with Sprite_Alloc(3), one 8-pixel cell per digit from pixel
 * (*x*, *y*), without leading zeros: digit d gets tile 0x40 + 2 * d and palette *palette*.
 * The y byte is written over the low byte of the attribute word after it is stored.
 */
void Vs_DrawNumber(s32 x, s32 y, u16 value, s32 palette)
{
    u32 n = value;
    u32 div = 10000;
    s32 started = FALSE;
    s32 digit;
    struct Sprite *e;
    struct Sprite *last; // its own variable: with e, agbcc picks r3 instead of r2

    do
    {
        for (digit = 0; n >= div; n -= div)
            digit++;
        if (digit != 0 || started)
        {
            started = TRUE;
            e = Sprite_Alloc(3);
            if (e != NULL)
            {
                e->attr01 = ((x << 16) & 0x1FF0000) | 0x8000;
                e->attr2 = (palette << 12) | (0x40 + digit * 2);
                *(u8 *)&e->attr01 = y;
            }
            x += 8;
        }
        div /= 10;
    } while (div > 9);
    last = Sprite_Alloc(3);
    if (last != NULL)
    {
        last->attr01 = ((x << 16) & 0x1FF0000) | 0x8000;
        last->attr2 = (palette << 12) | (0x40 + n * 2);
        *(u8 *)&last->attr01 = y;
    }
}
/**
 * A spinning sprite (affine set from Sprite_AllocAffine, turned by gMain.frameCount * 360 / 256)
 * at x = a * 240 / b, y 0x48: attribute 2 0x6AC (tile 0x2AC, priority 1) in list 4. Vs_RunMenus
 * calls it with the bytes of a link transfer sent so far and their total, under a blinking
 * "WAIT": a progress marker.
 */
void Vs_DrawSpinner(s32 a, s32 b)
{
    s32 n;
    struct Sprite *spr;
    u32 affine;
    u32 attr;

    n = Sprite_AllocAffine();
    if (n >= 0)
    {
#ifdef PLATFORM_PC
        Sprite_SetAffineRotation(n, (gMain.frameCount * 360) >> 8);
#else
        // The original passes the direction unnarrowed: this call saw no u8 prototype (an
        // implicit declaration or an int parameter); Sprite_SetAffineRotation narrows it
        // itself, so the result is the same. The PC port calls it through its prototype (a
        // call through another function type is undefined in C).
        ((void (*)(s32, s32))Sprite_SetAffineRotation)(n, (gMain.frameCount * 360) >> 8);
#endif
    }
    spr = Sprite_Alloc(4);
    if (spr != NULL)
    {
        // Two temporaries: the original ORs the constant into the position, not into the
        // affine index (one expression lets agbcc combine 0x40000100 with n << 25).
        affine = n << 25;
#ifdef PLATFORM_PC
        // A division by 0 (b, the transfer's size, is 0 when no guest needs the level data)
        // gives 0 on the GBA (agbcc's libgcc, lib1thumb.asm: Ldiv0) and traps on an x86.
        attr = (((b != 0 ? a * 240 / b : 0) << 16) & 0x1FF0000) | 0x40000100;
#else
        attr = (((a * 240 / b) << 16) & 0x1FF0000) | 0x40000100;
#endif
        spr->attr01 = affine | attr;
        // Halfword and byte stores through casts: like the original, the cast store loads
        // 0x6AC into r1 and copies it to r0; a u16 field store loads it straight into r0.
        ((u16 *)spr)[2] = 0x6AC;
        ((u8 *)spr)[0] = 0x48;
    }
}
/**
 * Fills gVsCourseOrder with 0..54, resets its index gVsCourseIndex and shuffles it with 55
 * calls of Vs_NextCourse.
 */
void Vs_ShuffleCourses(void)
{
    s32 *index = &gVsCourseIndex; // taken first: agbcc then loads its address first, as the original does
    u8 *p = gVsCourseOrder;
    s32 i;

    for (i = 54, p += i; i >= 0; i--)
        *p-- = i;
    *index = 0;
    for (i = 54; i >= 0; i--)
        Vs_NextCourse();
}
/**
 * Returns gVsCourseOrder[gVsCourseIndex], then swaps two entries of gVsCourseOrder picked by
 * Random_Next (each scaled to 0..gVsCourseIndex) and advances gVsCourseIndex (0 after 54).
 */
u8 Vs_NextCourse(void)
{
    u8 result = gVsCourseOrder[gVsCourseIndex];
    u8 a = (Random_Next() * gVsCourseIndex) >> 8;
    u8 b = (Random_Next() * gVsCourseIndex) >> 8;
    u8 tmp = gVsCourseOrder[a];

    gVsCourseOrder[a] = gVsCourseOrder[b];
    gVsCourseOrder[b] = tmp;
    if (++gVsCourseIndex > 54)
        gVsCourseIndex = 0;
    return result;
}
