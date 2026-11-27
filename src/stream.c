/* River City Quest (SNES), stage 13: the map at double size, streamed.

   The game still thinks in 16 px cells (movement, collision, everything placed on the map), but the screen shows
   every cell 32 px wide: four by four 8x8 tiles, a metatile. A map is a grid of metatile numbers (sGrid, in RAM so
   it can change as chests open and bushes go), a table of metatiles (ROM, 16 tilemap words each) and its tiles
   (ROM: up to 2,048, more than BG1's 1,024 slots). So only what's on screen is in VRAM: the tilemap is a 64x64
   ring that holds the 33x29 tiles around the camera, and each slot of the tile cache counts the loaded tiles using
   it. Moving a tile's width loads a column (or row) and drops the one leaving; a tile that isn't cached takes a
   free slot and is copied in at the next VBlank (src/stream.asm). 33x29 tiles can use at most 957 different
   tiles, so the cache can't run out, short of a map pinning many animated tiles (then a blank tile is drawn).

   Map words: bits 0-10 the tile, 11-13 the palette, 14-15 the flips. */
#include <snes.h>
#include "game.h"

#define SLOTS 1024
#define LW 33
#define LH 29
#define UPQ 72
#define WRQ 72

extern u16 sGrid[], sSlotOf[], sRef[], sFree[];
extern u16 upAddr[], upDst[], wrAddr[], wrVal[], tsAddr[];
extern u8 upBank[], tsBank[];
extern u16 upN, wrN, scrX, scrY;
void streamSetTs(u8 k, u8 *chunk);
void streamVBlank(void);

u8 *sMeta;
u16 sTW, sTH;   /* map size in tiles */
u8 sCW;         /* and its width in cells */
u16 sL, sT;     /* the loaded area's top-left tile */
u8 sLive;       /* the ring holds the loaded area (off between maps and in the minigames) */
u16 sFreeTop;
u8 sBlank;      /* forced blank: a full queue is written out at once */

static void flushNow(void)
{
    streamVBlank();
}

static u16 tileWord(u16 tx, u16 ty)
{
    u16 m = sGrid[(ty >> 2) * sCW + (tx >> 2)];
    u8 *p = sMeta + (m << 5) + (((ty & 3) << 2) + (tx & 3)) * 2;
    return p[0] | (p[1] << 8);
}

static void queueUp(u16 id, u16 slot)
{
    u16 n = upN;
    if (n >= UPQ)
    {
        if (!sBlank) return; /* (can't happen: a frame loads a column and a row at most) */
        flushNow();
        n = 0;
    }
    upAddr[n] = tsAddr[id >> 10] + ((id & 1023) << 5);
    upBank[n] = tsBank[id >> 10];
    upDst[n] = VRAM_BG1_CHR + (slot << 4);
    upN = n + 1;
}

static void queueWr(u16 addr, u16 v)
{
    u16 n = wrN;
    if (n >= WRQ)
    {
        if (!sBlank) return;
        flushNow();
        n = 0;
    }
    wrAddr[n] = addr;
    wrVal[n] = v;
    wrN = n + 1;
}

/* a map word -> the tilemap word, caching its tile */
static u16 slotFor(u16 w)
{
    u16 id = w & 0x7FF, s = sSlotOf[id];
    if (!s)
    {
        if (!sFreeTop) return 0;
        s = sFree[--sFreeTop];
        sSlotOf[id] = s;
        queueUp(id, s);
    }
    sRef[s]++;
    return s | ((w >> 1) & 0x1C00) | (w & 0xC000);
}

static void slotDrop(u16 w)
{
    u16 id = w & 0x7FF, s = sSlotOf[id];
    if (!s) return;
    if (--sRef[s] == 0)
    {
        sSlotOf[id] = 0;
        sFree[sFreeTop++] = s;
    }
}

static u16 ringAddr(u16 tx, u16 ty)
{
    return VRAM_BG1_MAP + ((ty & 32) << 6) + ((tx & 32) << 5) + ((ty & 31) << 5) + (tx & 31);
}

static void putTile(u16 tx, u16 ty)
{
    queueWr(ringAddr(tx, ty), (tx < sTW && ty < sTH) ? slotFor(tileWord(tx, ty)) : 0);
}

static void dropTile(u16 tx, u16 ty)
{
    if (tx < sTW && ty < sTH) slotDrop(tileWord(tx, ty));
}

/* ---- from the map loader ---- */

/* a new map: its tile set (two 32 KB chunks), metatiles and grid; w, h in 16 px cells */
void streamInit(u8 *ts0, u8 *ts1, u8 *meta, u8 *grid, u8 w, u8 h)
{
    u16 i, n = (u16)w * h;
    sLive = 0;
    streamSetTs(0, ts0);
    streamSetTs(1, ts1);
    sMeta = meta;
    sCW = w;
    sTW = (u16)w << 2;
    sTH = (u16)h << 2;
    for (i = 0; i < n; i++) sGrid[i] = grid[i * 2] | (grid[i * 2 + 1] << 8);
    for (i = 0; i < 2048; i++) sSlotOf[i] = 0;
    for (i = 0; i < SLOTS; i++) sRef[i] = 0;
    upN = wrN = 0;
    sFreeTop = 0;
}

/* the animated tiles (ids first.. first+count-1) live in the top slots for good; the rest are the cache.
   Returns the first pinned slot. */
u16 streamPin(u16 first, u16 count)
{
    u16 i, s0 = SLOTS - count;
    sBlank = 1;
    queueUp(0, 0);   /* slot 0: the blank tile (tile 0 of every map), for outside the map */
    for (i = 0; i < count; i++)
    {
        sSlotOf[first + i] = s0 + i;
        sRef[s0 + i] = 0x4000;
        queueUp(first + i, s0 + i);
    }
    sFreeTop = 0;
    for (i = s0 - 1; i > 0; i--) sFree[sFreeTop++] = i;   /* slot 0 stays blank */
    return s0;
}

/* forced blank: the whole loaded area, from the camera */
void streamFull(void)
{
    u16 x, y;
    sBlank = 1;
    /* (anything still loaded from before is forgotten: refcounts start from the pins) */
    sL = camX >> 2;
    sT = camY >> 2;
    for (y = 0; y < LH; y++)
        for (x = 0; x < LW; x++) putTile(sL + x, sT + y);
    flushNow();
    scrX = camX << 1;
    scrY = camY << 1;
    sLive = 1;   /* (sBlank stays on until the first scrolled frame: the room's chests and doors are set up meanwhile) */
}

/* each frame: a column and a row at most */
void streamUpdate(void)
{
    u16 i, nl = camX >> 2, nt = camY >> 2;
    sBlank = 0;
    if (nl > sL)
    {
        for (i = 0; i < LH; i++) dropTile(sL, sT + i);
        sL++;
        for (i = 0; i < LH; i++) putTile(sL + LW - 1, sT + i);
    }
    else if (nl < sL)
    {
        for (i = 0; i < LH; i++) dropTile(sL + LW - 1, sT + i);
        sL--;
        for (i = 0; i < LH; i++) putTile(sL, sT + i);
    }
    if (nt > sT)
    {
        for (i = 0; i < LW; i++) dropTile(sL + i, sT);
        sT++;
        for (i = 0; i < LW; i++) putTile(sL + i, sT + LH - 1);
    }
    else if (nt < sT)
    {
        for (i = 0; i < LW; i++) dropTile(sL + i, sT + LH - 1);
        sT--;
        for (i = 0; i < LW; i++) putTile(sL + i, sT);
    }
    scrX = camX << 1;
    scrY = camY << 1;
}

/* a cell changes (a chest opens, a bush is cut, a door unlocks): its metatile, and its tiles if loaded */
void streamCell(u8 cx, u8 cy, u16 meta)
{
    u16 tx, ty, x0 = (u16)cx << 2, y0 = (u16)cy << 2;
    u8 i, j, live = sLive;
    if (live)
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++)
            {
                tx = x0 + i;
                ty = y0 + j;
                if (tx >= sL && tx < sL + LW && ty >= sT && ty < sT + LH) dropTile(tx, ty);
            }
    sGrid[cy * sCW + cx] = meta;
    if (live)
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4; i++)
            {
                tx = x0 + i;
                ty = y0 + j;
                if (tx >= sL && tx < sL + LW && ty >= sT && ty < sT + LH) putTile(tx, ty);
            }
}

/* the minigames, album and title use BG1 their own way */
void streamStop(void)
{
    sLive = 0;
    upN = wrN = 0;
}
