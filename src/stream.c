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
#define LW 35   /* the loaded area: the 33x29 tiles a screen can touch and one more all round, */
#define LH 31   /* so a new edge is loaded a few frames at a time before it comes into view */
#define CHUNK 10 /* tiles of an edge loaded a frame (walking needs one edge every 3 frames) */
#define WRQ 96
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
static u16 tgtL, tgtT;   /* where the loaded area should be: one tile up and left of the camera's */
static u8 opK;           /* the edge being loaded: 0 none, 1 east (sL goes up), 2 west, 3 south, 4 north */
static u16 opI;          /* how much of it is done */

static void target(void)
{
    u16 l = camX >> 2, t = camY >> 2;
    tgtL = l ? l - 1 : 0;
    tgtT = t ? t - 1 : 0;
}

void streamFull(void)
{
    u16 y;
    sBlank = 1;
    /* (anything still loaded from before is forgotten: refcounts start from the pins) */
    target();
    sL = tgtL;
    sT = tgtT;
    opK = 0;
    for (y = 0; y < LH; y++)
    {
        stLine(sL, sT + y, 1, 0, LW, 1);
        streamVBlank();   /* (a row at a time: the queues hold 96) */
    }
    scrX = camX << 1;
    scrY = camY << 1;
    sLive = 1;   /* (sBlank stays on until the first scrolled frame: the room's chests and doors are set up meanwhile) */
}

/* up to n tiles more of the edge being loaded (each: the opposite edge's tile dropped, the new one put);
   returns how many */
static u16 opStep(u16 n)
{
    u16 len = opK <= 2 ? LH : LW, m = len - opI, room = WRQ - wrN;
    if (UPQ - upN < room) room = UPQ - upN;
    if (n > m) n = m;
    if (n > room) n = room;
    if (!n) return 0;
    switch (opK)
    {
    case 1:
        stLine(sL, sT + opI, 0, 1, n, 0);
        stLine(sL + LW, sT + opI, 0, 1, n, 1);
        break;
    case 2:
        stLine(sL + LW - 1, sT + opI, 0, 1, n, 0);
        stLine(sL - 1, sT + opI, 0, 1, n, 1);
        break;
    case 3:
        stLine(sL + opI, sT, 1, 0, n, 0);
        stLine(sL + opI, sT + LH, 1, 0, n, 1);
        break;
    default:
        stLine(sL + opI, sT + LH - 1, 1, 0, n, 0);
        stLine(sL + opI, sT - 1, 1, 0, n, 1);
        break;
    }
    opI += n;
    if (opI >= len)
    {
        if (opK == 1) sL++;
        else if (opK == 2) sL--;
        else if (opK == 3) sT++;
        else sT--;
        opK = 0;
    }
    return n;
}

/* is the screen still inside the loaded area (with the edge under way not counted)? */
static u8 urgent(void)
{
    return sL + 1 < tgtL || tgtL + 1 < sL || sT + 1 < tgtT || tgtT + 1 < sT;
}

/* each frame: CHUNK tiles of edge, more if the screen is about to show something not loaded */
void streamUpdate(void)
{
    u16 budget = CHUNK, used;
    u8 hurry;
    sBlank = 0;
    target();
    for (;;)
    {
        if (!opK)
        {
            if (sL < tgtL) opK = 1;
            else if (sL > tgtL) opK = 2;
            else if (sT < tgtT) opK = 3;
            else if (sT > tgtT) opK = 4;
            else break;
            opI = 0;
        }
        hurry = urgent();
        used = opStep(hurry ? 255 : budget);
        if (!used) break;   /* (the queues are full) */
        if (!hurry)
        {
            if (used >= budget) break;
            budget -= used;
        }
    }
    scrX = camX << 1;
    scrY = camY << 1;
}

/* finish the edge under way (before a cell changes: the loaded area must be one rectangle) */
static void opFinish(void)
{
    while (opK)
        if (!opStep(255))
        {
            if (!sBlank) return;   /* (can't happen: the queues hold more than an edge and a cell) */
            streamVBlank();
        }
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
    if (live)
    {
        opFinish();
        cellRows(x0, y0, 0);
    }
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
