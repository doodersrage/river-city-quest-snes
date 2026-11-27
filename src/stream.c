/* River City Quest (SNES), stage 13: the map at double size, streamed.

   The game still thinks in 16 px cells (movement, collision, everything placed on the map), but the screen shows
   every cell 32 px wide: four by four 8x8 tiles, a metatile. A map is a grid of metatile numbers (sGrid, in RAM so
   it can change as chests open and bushes go), a table of metatiles (ROM, 16 tilemap words each) and its tiles
   (ROM: up to 2,048, more than BG1's 1,024 slots). So only what's on screen is in VRAM: the tilemap is a 64x64
   ring that holds the 33x29 tiles around the camera, and each slot of the tile cache counts the loaded tiles using
   it. Moving a tile's width loads a column (or row) and drops the one leaving; a tile that isn't cached takes a
   free slot and is copied in at the next VBlank (src/streamvb.asm). 33x29 tiles can use at most 957 different
   tiles, so the cache can't run out, short of a map pinning many animated tiles (then a blank tile is drawn).

   Map words: bits 0-10 the tile, 11-13 the palette, 14-15 the flips. */
#include <snes.h>
#include "game.h"

#define SLOTS 1024
#define LW 33
#define LH 29
#define UPQ 96

extern u16 sGrid[], sSlotOf[], sRef[], sFree[], gridRow[];
extern u16 upAddr[], upDst[], tsAddr[];
extern u8 upBank[], tsBank[];
extern u16 upN, wrN, scrX, scrY, stFreeTop, stTW, stTH;
void streamSetTs(u8 k, u8 *chunk);
void streamSetMeta(u8 *meta);
void streamVBlank(void);
/* (src/streamvb.asm) n tiles from (tx, ty) stepping (dx, dy): put 1 loads them, 0 drops them */
void stLine(u16 tx, u16 ty, u16 dx, u16 dy, u16 n, u8 put);
void streamReset(u8 *grid, u16 cells, u16 s0);
u8 *sGridSrc;
u16 sCells;

u8 sCW;         /* the map's width in cells */
u16 sL, sT;     /* the loaded area's top-left tile */
u8 sLive;       /* the ring holds the loaded area (off between maps and in the minigames) */
u8 sBlank;      /* forced blank */

static void queueUp(u16 id, u16 slot)
{
    u16 n = upN;
    if (n >= UPQ)
    {
        streamVBlank();
        n = 0;
    }
    upAddr[n] = tsAddr[id >> 10] + ((id & 1023) << 5);
    upBank[n] = tsBank[id >> 10];
    upDst[n] = VRAM_BG1_CHR + (slot << 4);
    upN = n + 1;
}

/* ---- from the map loader ---- */

/* a new map: its tile set (two 32 KB chunks), metatiles and grid; w, h in 16 px cells */
void streamInit(u8 *ts0, u8 *ts1, u8 *meta, u8 *grid, u8 w, u8 h)
{
    u16 i, r;
    sLive = 0;
    streamSetTs(0, ts0);
    streamSetTs(1, ts1);
    streamSetMeta(meta);
    sCW = w;
    stTW = (u16)w << 2;
    stTH = (u16)h << 2;
    sGridSrc = grid;
    sCells = (u16)w * h;
    for (i = 0, r = 0; i < 64; i++, r += (u16)w * 2) gridRow[i] = r;
    upN = wrN = 0;
}

/* the animated tiles (ids first.. first+count-1) live in the top slots for good; the rest are the cache.
   Returns the first pinned slot. */
u16 streamPin(u16 first, u16 count)
{
    u16 i, s0 = SLOTS - count;
    sBlank = 1;
    streamReset(sGridSrc, sCells, s0);   /* (the grid, an empty cache and the free stack) */
    queueUp(0, 0);   /* slot 0: the blank tile (tile 0 of every map), for outside the map */
    for (i = 0; i < count; i++)
    {
        sSlotOf[first + i] = s0 + i;
        sRef[s0 + i] = 0x4000;
        queueUp(first + i, s0 + i);
    }
    return s0;
}

/* forced blank: the whole loaded area, from the camera */
void streamFull(void)
{
    u16 y;
    sBlank = 1;
    /* (anything still loaded from before is forgotten: refcounts start from the pins) */
    sL = camX >> 2;
    sT = camY >> 2;
    for (y = 0; y < LH; y++)
    {
        stLine(sL, sT + y, 1, 0, LW, 1);
        streamVBlank();   /* (a row at a time: the queues hold 96) */
    }
    scrX = camX << 1;
    scrY = camY << 1;
    sLive = 1;   /* (sBlank stays on until the first scrolled frame: the room's chests and doors are set up meanwhile) */
}

/* each frame: a column and a row at most */
void streamUpdate(void)
{
    u16 nl = camX >> 2, nt = camY >> 2;
    sBlank = 0;
    if (nl > sL)
    {
        stLine(sL, sT, 0, 1, LH, 0);
        sL++;
        stLine(sL + LW - 1, sT, 0, 1, LH, 1);
    }
    else if (nl < sL)
    {
        stLine(sL + LW - 1, sT, 0, 1, LH, 0);
        sL--;
        stLine(sL, sT, 0, 1, LH, 1);
    }
    if (nt > sT)
    {
        stLine(sL, sT, 1, 0, LW, 0);
        sT++;
        stLine(sL, sT + LH - 1, 1, 0, LW, 1);
    }
    else if (nt < sT)
    {
        stLine(sL, sT + LH - 1, 1, 0, LW, 0);
        sT--;
        stLine(sL, sT, 1, 0, LW, 1);
    }
    scrX = camX << 1;
    scrY = camY << 1;
}

/* a cell changes (a chest opens, a bush is cut, a door unlocks): its metatile, and its tiles if loaded */
static void cellRows(u16 x0, u16 y0, u8 put)
{
    u16 a = x0, b = x0 + 4, y;
    if (a < sL) a = sL;
    if (b > sL + LW) b = sL + LW;
    if (a >= b) return;
    for (y = y0; y < y0 + 4; y++)
        if (y >= sT && y < sT + LH) stLine(a, y, 1, 0, b - a, put);
}

void streamCell(u8 cx, u8 cy, u16 meta)
{
    u16 x0 = (u16)cx << 2, y0 = (u16)cy << 2;
    u8 live = sLive;
    if (live) cellRows(x0, y0, 0);
    sGrid[cy * sCW + cx] = meta;
    if (live)
    {
        cellRows(x0, y0, 1);
        if (sBlank) streamVBlank();
    }
}

/* the minigames, album and title use BG1 their own way */
void streamStop(void)
{
    sLive = 0;
    upN = wrN = 0;
}
