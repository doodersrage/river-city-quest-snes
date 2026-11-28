/* River City Quest (SNES): three save slots in battery-backed SRAM (stage 11). The game saves itself each time
   you arrive somewhere, like the web game's autosave. A slot is a magic word, then everything that makes up
   your progress in one fixed order, then a checksum; anything that doesn't check out reads as an empty slot. */
#include <snes.h>
#include "game.h"

#define SLOT_SIZE 1024
#define MAGIC0 'R'
#define MAGIC1 'Q'   /* the first layout (stage 11) */
#define MAGIC1X 'X'  /* stage 15: the same, then the extension block */
#define EXT_SIZE 128 /* the extension block: fields added from stage 15 on, the rest zero, so it can grow without moving */

extern u8 linkOpen[], roomBraz[], photoGot[], fishN[];
extern u8 shopOwned[2][MAX_SHOP_ITEMS];
extern u8 retMap, retX, retY;
extern u8 hasCan;

u8 saveSlot;          /* 1-3, 0 none */
char heroName[9];
u16 playMin, playSub; /* time played: minutes, and frames toward the next */
u8 saveBuf[SLOT_SIZE];
static u16 sp, saveLen, baseLen;
static u8 svExt;
extern u8 medals;
static u8 svMode; /* 0 load, 1 save, 2 only count */

static void sv(u8 *v, u16 n)
{
    u16 i;
    if (svMode == 1)
        for (i = 0; i < n; i++) saveBuf[sp + i] = v[i];
    else if (svMode == 0)
        for (i = 0; i < n; i++) v[i] = saveBuf[sp + i];
    sp += n;
}

/* everything that is your progress. The first three are read by the title screen at fixed places. */
static void saveWalk(void)
{
    sp = 2;
    sv((u8 *)heroName, 9);   /* 2 */
    sv((u8 *)&playMin, 2);   /* 11 */
    sv(&hpMax, 1);           /* 13 */
    sv((u8 *)&pins, 2);      /* 14 */
    sv(&hp, 1);
    sv(&curMap, 1);
    sv((u8 *)&px, 2);
    sv((u8 *)&py, 2);
    sv(&heroDir, 1);
    sv(&retMap, 1);
    sv(&retX, 1);
    sv(&retY, 1);
    sv(&dentMap, 1);
    sv((u8 *)&coins, 2);
    sv(chestOpen, (CHEST_COUNT + 7) / 8);
    sv(linkOpen, (LINK_COUNT + 7) / 8);
    sv(pickupGot, (PICKUP_COUNT + 7) / 8);
    sv(roomFlags, MAP_COUNT);
    sv(roomBraz, MAP_COUNT);
    sv(keys, AREA_COUNT);
    sv((u8 *)shopOwned, 2 * MAX_SHOP_ITEMS);
    sv(&hasHook, 1);
    sv(&hasPowder, 1);
    sv(&hasLantern, 1);
    sv(&hasShield, 1);
    sv(&hasBug, 1);
    sv(&hasCan, 1);
    sv(&powder, 1);
    sv(&eqItem, 1);
    sv(&hasCarkey, 1);
    sv(&pieces, 1);
    sv(&hasKayak, 1);
    sv(&tradeItem, 1);
    sv(&melonTaken, 1);
    sv(&parts, 1);
    sv(&bateauDone, 1);
    sv(&hasLure, 1);
    sv(&keystones, 1);
    sv(&gameWon, 1);
    sv(&hasBalls, 1);
    sv(&hasShoes, 1);
    sv(&hasBike, 1);
    sv(&hasBottle, 1);
    sv(&hasBib, 1);
    sv(&bottleFull, 1);
    sv(&balls, 1);
    sv(&capBalls, 1);
    sv(&capPowder, 1);
    sv(&goldPaddle, 1);
    sv(loreRead, (LORE_COUNT + 7) / 8);
    sv(&hasRod, 1);
    sv(&hasCamera, 1);
    sv(&frogs, 1);
    sv(&frogsDone, 1);
    sv((u8 *)&cards, 2);
    sv(fishN, FISH_COUNT);
    sv(photoGot, (PHOTO_COUNT + 7) / 8);
    sv((u8 *)miniBest, 16);
    sv((u8 *)&dayClock, 2);
    if (!svExt) return;
    /* stage 15 extension: add new fields at the end only */
    {
        u16 e0 = sp;
        sv(&medals, 1);
        sp = e0 + EXT_SIZE;
    }
}

static u16 checksum(u16 n)
{
    u16 i, c = 0x1234;
    for (i = 0; i < n; i++) c = (c << 1) + (c >> 15) + saveBuf[i];
    return c;
}

/* work out the slot length once, at power-on */
void saveInit(void)
{
    svMode = 2;
    svExt = 0;
    saveWalk();
    baseLen = sp;
    svExt = 1;
    saveWalk();
    saveLen = sp;
    saveSlot = 0;
    playMin = playSub = 0;
}

void saveGame(void)
{
    u16 c;
    if (!saveSlot) return;
    svMode = 1;
    svExt = 1;
    for (c = baseLen; c < saveLen; c++) saveBuf[c] = 0;
    saveWalk();
    saveBuf[0] = MAGIC0;
    saveBuf[1] = MAGIC1X;
    c = checksum(saveLen);
    saveBuf[saveLen] = c & 0xFF;
    saveBuf[saveLen + 1] = c >> 8;
    consoleCopySramWithOffset(saveBuf, saveLen + 2, (saveSlot - 1) * SLOT_SIZE);
}

/* read a slot into the buffer: 1 if it holds a good save */
u8 slotOk(u8 slot)
{
    u16 c;
    u16 n;
    consoleLoadSramWithOffset(saveBuf, saveLen + 2, (slot - 1) * SLOT_SIZE);
    if (saveBuf[0] != MAGIC0 || (saveBuf[1] != MAGIC1 && saveBuf[1] != MAGIC1X)) return 0;
    n = saveBuf[1] == MAGIC1X ? saveLen : baseLen; /* a save from before stage 15 has no extension */
    c = checksum(n);
    svExt = saveBuf[1] == MAGIC1X;
    return saveBuf[n] == (c & 0xFF) && saveBuf[n + 1] == (c >> 8);
}

/* start the game from a slot (one the title saw was good) */
void loadGame(u8 slot)
{
    slotOk(slot); /* the buffer may hold another slot by now; sets svExt */
    if (!svExt)
    {
        u16 i;
        for (i = baseLen; i < saveLen; i++) saveBuf[i] = 0; /* an old save: the new fields start empty */
    }
    svExt = 1;
    svMode = 0;
    saveWalk();
    heroName[8] = 0;
    playSub = 0;
    saveSlot = slot;
}

/* wipe a slot */
void eraseSlot(u8 slot)
{
    saveBuf[0] = saveBuf[1] = 0;
    consoleCopySramWithOffset(saveBuf, 2, (slot - 1) * SLOT_SIZE);
}

void playTick(void)
{
    if (++playSub >= 3600)
    {
        playSub = 0;
        if (playMin < 59999) playMin++;
    }
}
