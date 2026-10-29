// SPDX-License-Identifier: CC0-1.0 (this project's contributions only: see NOTICE.md)
// save: the save data: boot load, save, file and record reset.
// 08005178-080057B8, split off the provisional unit src/game.c (docs/modules.md).

#include "game.h"
#include "main.h"
#include "bg.h"
#include "sprite.h"
#include "text.h"
#include "menu.h"
#include "save.h"

extern const char gTextNoRecordName[9] __attribute__((aligned(4)));

void ReadSram(const u8 *src, u8 *dest, u32 size);
void WriteSram(const u8 *src, u8 *dest, u32 size);
void Save_ResetFiles(void);
void Save_ResetRecords(void);
// The save's signature (16 bytes at 0x0E000000) and the texts Save_LoadAtBoot draws
extern const char gSaveSignature[];
extern const u8 gTextDeleteSave1[];
extern const u8 gTextDeleteSave2[];
extern const u8 gTextYes[];
extern const u8 gTextNo[];
extern const u8 gTextSaveDamaged1[];
extern const u8 gTextSaveDamaged2[];
extern const u8 gTextSaveDamaged3[];

// The save data in SRAM: the signature gSaveSignature, the bank in use (0/1), each bank's
// checksum, and the two banks of 0x2254 bytes (gSaveBank's data)
#define SRAM_SIGNATURE ((u8 *)0x0E000000)
#define SRAM_BANK ((u8 *)0x0E000010)
#define SRAM_CHECKSUM(bank) ((u8 *)0x0E000012 + (bank) * 2)
#define SRAM_DATA(bank) ((u8 *)0x0E000018 + (bank) * 0x2254)

/**
 * Loads the save at boot. With A, B, Select and Start held it first asks whether to erase it
 * (two choices, Menu_StepValueUpDownWrap; the first erases). Otherwise it reads the bank in use and
 * checks its checksum (the sum of its 0x2254 bytes), then the other bank's; if the
 * signature differs it formats the save, and if neither bank is good it says so (waiting
 * for A) and formats it: new records (Save_ResetFiles, Save_ResetRecords), the signature, bank 0.
 * Every way ends in Save_Write (twice after a format).
 */
void Save_LoadAtBoot(void)
{
    char header[16];
    u16 choice;
    u16 bank;
    u16 sum;
    u16 calc;
    const u8 *p;
    u32 i;
    s32 n;

    if ((gMain.heldKeys & 0xF) == 0xF)
    {
        Bg_SetVideoMode(0);
        gDispcntCopy = 0x1040;
        Main_QueueCopy(gMenuObjTiles, (void *)0x06012400, 0x4680);
        Main_QueueCopy(gLevelSpritePalettes, (void *)0x05000200, 0x200);
        Main_EndFrame();
        choice = 1;
        while (!(gMain.newKeys & A_BUTTON))
        {
            Sprite_ClearLists();
            Menu_StepValueUpDownWrap(&choice, 1, gMain.newKeys);
            gTextCursor.y = 0x30;
            gTextCursor.dx = 8;
            gTextCursor.dy = 0x10;
            gTextCursor.palette = 0xE;
            gTextCursor.list = 0;
            gTextCursor.x = 0x24;
            Text_PrintProportional(&gTextCursor, gTextDeleteSave1);
            gTextCursor.x = 0x2C;
            Text_PrintProportional(&gTextCursor, gTextDeleteSave2);
            // the chosen answer blinks
            if ((gMain.frameCount & 8) && choice == 0)
                gTextCursor.palette = 0xE;
            else
                gTextCursor.palette = 0xF;
            gTextCursor.x = 0x6C;
            Text_PrintProportional(&gTextCursor, gTextYes);
            if ((gMain.frameCount & 8) && choice != 0)
                gTextCursor.palette = 0xE;
            else
                gTextCursor.palette = 0xF;
            gTextCursor.x = 0x70;
            Text_PrintProportional(&gTextCursor, gTextNo);
            Sprite_BuildOam();
            Main_EndFrame();
        }
        m4aSongNumStart(0x21);
        for (n = 60; n > 0; n--)
            Main_EndFrame();
        if (choice == 0)
        {
            Save_ResetFiles();
            Save_ResetRecords();
            WriteSram((const u8 *)gSaveSignature, SRAM_SIGNATURE, 0x10);
            bank = 0;
            WriteSram((u8 *)&bank, SRAM_BANK, 2);
            Save_Write();
            Save_Write();
            return;
        }
    }

    ReadSram(SRAM_SIGNATURE, (u8 *)header, 0x10);
    ReadSram(SRAM_BANK, (u8 *)&bank, 2);
    if (strcmp(gSaveSignature, header) != 0)
        goto format;
    if (bank <= 1)
    {
        ReadSram(SRAM_DATA(bank), gSaveBank, 0x2254);
        ReadSram(SRAM_CHECKSUM(bank), (u8 *)&sum, 2);
        calc = 0;
        p = gSaveBank;
        for (i = 0; i <= 0x2253; i++)
            calc += *p++;
        if (sum == calc)
        {
            Save_Write();
            return;
        }
        bank = (bank == 0) ? 1 : 0;
        ReadSram(SRAM_DATA(bank), gSaveBank, 0x2254);
        ReadSram(SRAM_CHECKSUM(bank), (u8 *)&sum, 2);
        calc = 0;
        p = gSaveBank;
        for (i = 0; i <= 0x2253; i++)
            calc += *p++;
        if (sum == calc)
        {
            Save_Write();
            return;
        }
    }

    // neither bank is good: say so until A
    Bg_SetVideoMode(0);
    gDispcntCopy = 0x1040;
    Main_QueueCopy(gMenuObjTiles, (void *)0x06012400, 0x4680);
    Main_QueueCopy(gLevelSpritePalettes, (void *)0x05000200, 0x200);
    Main_EndFrame();
    while (!(gMain.newKeys & A_BUTTON))
    {
        Sprite_ClearLists();
        gTextCursor.y = 0x38;
        gTextCursor.dx = 8;
        gTextCursor.dy = 0x10;
        gTextCursor.palette = 0xE;
        gTextCursor.list = 0;
        gTextCursor.x = 0x3C;
        Text_PrintProportional(&gTextCursor, gTextSaveDamaged1);
        gTextCursor.x = 0x4C;
        Text_PrintProportional(&gTextCursor, gTextSaveDamaged2);
        gTextCursor.x = 0x2C;
        Text_PrintProportional(&gTextCursor, gTextSaveDamaged3);
        Sprite_BuildOam();
        Main_EndFrame();
    }
    m4aSongNumStart(0x21);

format:
    for (n = 60; n > 0; n--)
        Main_EndFrame();
    Save_ResetFiles();
    Save_ResetRecords();
    WriteSram((const u8 *)gSaveSignature, SRAM_SIGNATURE, 0x10);
    bank = 0;
    WriteSram((u8 *)&bank, SRAM_BANK, 2);
    Save_Write();
    Save_Write();
}
u32 VerifySram(const u8 *src, u8 *dest, u32 size);

// SRAM as Save_LoadAtBoot's macros describe it, from one base (Save_Write addresses it so)
struct SramSave {
    /*0x0000*/ u8 signature[0x10];
    /*0x0010*/ u16 bank;
    /*0x0012*/ u16 checksum[2];
    /*0x0016*/ u8 unk16[2];
    /*0x0018*/ u8 data[2][0x2254];
};

/**
 * Saves gSaveBank's 0x2254 bytes to the bank not in use, then its checksum (the sum of
 * the bytes), then makes it the bank in use, verifying each write (VerifySram). Returns
 * the last verification's result (0: saved), or -1 when the data or the checksum did not
 * verify (the old bank stays in use).
 */
s32 Save_Write(void)
{
    struct SramSave *sram = (struct SramSave *)0x0E000000;
    u16 bank;
    u16 sum;
    const u8 *p;
    u32 i;

    ReadSram((u8 *)&sram->bank, (u8 *)&bank, 2);
    bank = (bank == 0) ? 1 : 0;
    sum = 0;
    p = gSaveBank;
    for (i = 0; i <= 0x2253; i++)
        sum += *p++;
    WriteSram(gSaveBank, sram->data[bank], 0x2254);
    if (VerifySram(gSaveBank, sram->data[bank], 0x2254) == 0)
    {
        WriteSram((u8 *)&sum, (u8 *)&sram->checksum[bank], 2);
        if (VerifySram((u8 *)&sum, (u8 *)&sram->checksum[bank], 2) == 0)
        {
            WriteSram((u8 *)&bank, (u8 *)&sram->bank, 2);
            return VerifySram((u8 *)&bank, (u8 *)&sram->bank, 2);
        }
    }
    return -1;
}
extern const char gTextGuest[];

/**
 * Resets save file *n* (the record at gSaveFiles + 0x1F4 * n): its number, an empty name
 * (gTextGuest's 6 bytes for file 0), flags and rank cleared, unlocks 0x20400, type-bonusTaken
 * cleared, every level's time 216000 frames (60 minutes) and mark 0.
 */
void Save_ResetFile(s32 n)
{
    struct SaveFile *file = (struct SaveFile *)(gSaveFiles + n * 0x1F4);
    s32 i;

    file->number = n;
    file->name[0] = 0;
    file->flags = 0;
    file->unlocks = 0x20400;
    file->type = 0;
    file->color = 0;
    file->items = 0;
    file->bonusTaken = 0;
    file->rank = 0;
    for (i = 0; i <= 0x5C; i++)
    {
        file->bestTimes[i] = 216000;
        file->marks[i] = 0;
    }
    if (n == 0)
        memcpy(file->name, gTextGuest, 6);
}
/**
 * Calls Save_ResetFile for the five save files 0-4 (it takes the record at gSaveFiles +
 * 0x1F4 * n).
 */
void Save_ResetFiles(void)
{
    s32 i;

    for (i = 0; i <= 4; i++)
        Save_ResetFile(i);
}
/**
 * Resets the records of *gSaveBank (three per level, from + 0x9C4 and + 0x1B34): every
 * time to 216000 frames (60 minutes) with the name gTextNoRecordName, then the first two of each
 * level to its gLevelTimes entries [1] and [0], named gDefaultRecordNames[1] / [0], with rank 2
 * and 1. Levels 35-37 and 88-92 of the first table (93 levels) and 35-37 of the second (38)
 * keep the reset; the second table takes its times from gEasyLevelTimes (KuruTools: the Easy
 * Mode times).
 */
void Save_ResetRecords(void)
{
    struct LevelRecord *rec;
    u32 *name;
    s32 i;

    rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
    for (i = 0; i < 0x117; i++)
    {
        rec->time = 216000;
        // Through u32 pointers agbcc knows both are word-aligned and copies the 9 bytes
        // inline (ldm/stm and a byte), as the original does: the reset runs just before
        // the save is written to SRAM, and a slower copy (a memcpy call per record) leaves
        // the save behind the original's at the end of the frame.
        name = (u32 *)rec->name;
        memcpy(name, gTextNoRecordName, sizeof(gTextNoRecordName));
        rec++;
    }
    rec = (struct LevelRecord *)(gSaveBank + 0x1B34);
    for (i = 0; i < 0x72; i++)
    {
        rec->time = 216000;
        name = (u32 *)rec->name;
        memcpy(name, gTextNoRecordName, sizeof(gTextNoRecordName));
        rec++;
    }

    rec = (struct LevelRecord *)(gSaveBank + 0x9C4);
    for (i = 0; i < 35; i++)
    {
        rec->time = gLevelTimes[i][1];
        strcpy(rec->name, gDefaultRecordNames[1]);
        rec->rank = 2;
        rec++;
        rec->time = gLevelTimes[i][0];
        strcpy(rec->name, gDefaultRecordNames[0]);
        rec->rank = 1;
        rec += 2;
    }
    rec = (struct LevelRecord *)(gSaveBank + 0x9C4) + 38 * 3;
    for (i = 38; i < 88; i++)
    {
        rec->time = gLevelTimes[i][1];
        strcpy(rec->name, gDefaultRecordNames[1]);
        rec->rank = 2;
        rec++;
        rec->time = gLevelTimes[i][0];
        strcpy(rec->name, gDefaultRecordNames[0]);
        rec->rank = 1;
        rec += 2;
    }
    rec = (struct LevelRecord *)(gSaveBank + 0x1B34);
    for (i = 0; i < 35; i++)
    {
        rec->time = gEasyLevelTimes[i][1];
        strcpy(rec->name, gDefaultRecordNames[1]);
        rec->rank = 2;
        rec++;
        rec->time = gEasyLevelTimes[i][0];
        strcpy(rec->name, gDefaultRecordNames[0]);
        rec->rank = 1;
        rec += 2;
    }
}
