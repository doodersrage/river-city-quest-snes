/* River City Quest (SNES): town life. Townspeople who wander about, cars and the Pulse bus that stop for you,
   and pigeons that take off when you come near (web updLife, drawCar). */
#include <snes.h>
#include "game.h"

#define OAM_CARS 40  /* stage 13c: the cars, 4 sprites a 32 px piece, 20 in all (a boss's and its shots' slots:
                        no map has both); the pigeons are 118 and 119 */
#define OAM_BIRDS 118
#define TOWN_OBJS 22
#define MAX_CARS 2
#define MAX_BIRDS 2

/* wandering townspeople: home spot, a new idea every 50-140 frames; one of them moves each frame */
s16 npcHx[MAX_NPCS], npcHy[MAX_NPCS];
s8 npcVx[MAX_NPCS], npcVy[MAX_NPCS];
u8 npcT[MAX_NPCS];
u8 wanderer[MAX_NPCS], nWander, wTurn;

/* cars: x in 1/16 px, lane y in px; carSp signed speed in 1/16 px a frame */
u8 carOn[MAX_CARS], carBus[MAX_CARS], carCol[MAX_CARS], carStop[MAX_CARS], carAny;
s8 carDir[MAX_CARS], carSp[MAX_CARS];
s16 carX[MAX_CARS], carY[MAX_CARS];

/* pigeons: 0 sitting, 1 flying, 2 gone, 255 none; x, y in 1/16 px */
u8 birdSt[MAX_BIRDS], birdV[MAX_BIRDS];
u16 birdT[MAX_BIRDS];
s16 birdX[MAX_BIRDS], birdY[MAX_BIRDS], birdVx[MAX_BIRDS], birdVy[MAX_BIRDS];
s16 birdHx[MAX_BIRDS], birdHy[MAX_BIRDS];

/* the town's 8 OAM slots: only rewrite one when it changes (most frames, nothing does) */
s16 lastSx[TOWN_OBJS], lastSy[TOWN_OBJS];
u16 lastTile[TOWN_OBJS];
u8 lastOn[TOWN_OBJS], lifeAny, townB;

static void townHide(u8 i)
{
    if (!lastOn[i]) return;
    lastOn[i] = 0;
    sprHide(i < 20 ? OAM_CARS + i : OAM_BIRDS + i - 20);
}

static void townPut(u8 i, s16 sx, s16 sy, u8 flip, u16 tile)
{
    if (sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
    {
        townHide(i);
        return;
    }
    tile |= flip << 15; /* remembered with the tile */
    if (lastOn[i] && lastSx[i] == sx && lastSy[i] == sy && lastTile[i] == tile) return;
    lastOn[i] = 1;
    lastSx[i] = sx;
    lastSy[i] = sy;
    lastTile[i] = tile;
    sprPut(i < 20 ? OAM_CARS + i : OAM_BIRDS + i - 20, 1, sx, sy, 2, flip, tile & 0x7FFF, PAL_ITEM);
}

static u8 carHalf(u8 k)
{
    return carBus[k] ? 20 : 13;
}

/* web carHit: a car or the bus fills its lane this wide */
u8 carHit(s16 x, s16 y)
{
    u8 k;
    s16 cx;
    if (!carAny) return 0;
    for (k = 0; k < MAX_CARS; k++)
        if (carOn[k] && ABS(y - carY[k]) < 10)
        {
            cx = carX[k] >> 4;
            if (ABS(x - cx) < carHalf(k) + 5) return 1;
        }
    return 0;
}

static s16 mapRight(void)
{
    return MAP_PX_W + 40;
}

/* Called from rcqUpload after the NPCs are placed (forced blank). */
void lifeLoad(void)
{
    u8 k, l;
    nWander = 0;
    wTurn = 0;
    for (k = 0; k < npcCount; k++)
        if (wld[WLD_NPCS + 4 + k * NPC_REC] & 0x80) wanderer[nWander++] = k;
    for (k = 0; k < MAX_NPCS; k++)
    {
        npcHx[k] = npcX[k];
        npcHy[k] = npcY[k];
        npcVx[k] = 0;
        npcVy[k] = 0;
        npcT[k] = rnd() % 60;
    }
    carAny = 0;
    for (k = 0; k < MAX_CARS; k++)
    {
        l = carLane[curMap * 2 + k];
        carOn[k] = l != 0;
        if (!l) continue;
        carAny = 1;
        carBus[k] = l >> 7;
        carDir[k] = (l & 0x40) ? -1 : 1;
        carSp[k] = carBus[k] ? 13 : 18;
        if (carDir[k] < 0) carSp[k] = -carSp[k];
        carY[k] = (l & 0x3F) * 16 + 8;
        carCol[k] = rnd() % 3;
        carStop[k] = 0;
        carX[k] = (s16)(rnd() % MAP_PX_W) << 4;
        /* never arrive inside a car */
        if (ABS(py - carY[k]) < 16 && ABS(px - (carX[k] >> 4)) < 48)
            carX[k] = (px < MAP_PX_W / 2 ? px + MAP_PX_W / 2 : px - MAP_PX_W / 2) << 4;
    }
    for (k = 0; k < MAX_BIRDS; k++)
    {
        l = birdTx[curMap * 2 + k];
        if (l == 255)
        {
            birdSt[k] = 255;
            continue;
        }
        birdHx[k] = (l * 16 + 8) << 4;
        birdHy[k] = (birdTy[curMap * 2 + k] * 16 + 10) << 4;
        birdX[k] = birdHx[k];
        birdY[k] = birdHy[k];
        birdSt[k] = 0;
        birdT[k] = rnd() % 60;
        birdV[k] = 1;
    }
    for (k = 0; k < TOWN_OBJS; k++)
    {
        lastOn[k] = 1;
        townHide(k);
    }
    chalLoad();
    lifeAny = carAny || birdSt[0] != 255 || birdSt[1] != 255;
    /* their art goes where a boss would load: outdoor maps never have one */
    if (!roomBoss)   /* anywhere without a boss: also the challenge tokens, the '!' and the camera icon */
    {
        /* sheet B has the bus (and one car colour), A two car colours */
        townB = (carOn[0] && carBus[0]) || (carOn[1] && carBus[1]);
        if (townB) dmaCopyVram(&townb_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&townb_chrend - &townb_chr));
        else dmaCopyVram(&town_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&town_chrend - &town_chr));
    }
}

static u8 npcFree(s16 x, s16 y)
{
    if (solidAt(x - 5, y - 5) || solidAt(x + 5, y - 5) || solidAt(x - 5, y + 5) || solidAt(x + 5, y + 5)) return 0;
    if (x < 8 || y < 8 || x > MAP_PX_W - 8 || y > MAP_PX_H - 8) return 0;
    if (ABS(x - px) < 13 && ABS(y - py) < 13) return 0;
    if (carHit(x, y)) return 0;
    return 1;
}

static void lifeTraffic(void)
{
    u8 k;
    s16 ah, cx;
    if (!carAny) return;
    /* cars stop for you when you're in the lane ahead (web: a short horn) */
    i = 0;
    for (k = 0; k < MAX_CARS; k++)
    {
        if (!carOn[k]) continue;
        /* stage 13c: 32 px tall, each old 16 px cell a 32 px piece of four sprites; drawn facing right, mirrored
           going left (the pieces in the other order, and each piece's halves swapped) */
        n = carBus[k] ? 3 : 2;
        base = carBus[k] ? 8 : (townB ? 0 : (carCol[k] & 1) << 3);
        flip = carDir[k] < 0;
        sx = SX((carX[k] >> 4) - (n << 3));
        sy = SY(carY[k]) - 22;
        for (c = 0; c < n; c++, sx += 32)
        {
            q = base + ((flip ? n - 1 - c : c) << 2);
            townPut(i++, sx + (flip ? 16 : 0), sy, flip, carTile[q]);
            townPut(i++, sx + (flip ? 0 : 16), sy, flip, carTile[q + 1]);
            townPut(i++, sx + (flip ? 16 : 0), sy + 16, flip, carTile[q + 2]);
            townPut(i++, sx + (flip ? 0 : 16), sy + 16, flip, carTile[q + 3]);
        }
    }
    while (i < 20) townHide(i++);
    for (k = 0; k < MAX_BIRDS; k++)
    {
        i = 20 + k;
        if (birdSt[k] > 1)
        {
            townHide(i);
            continue;
        }
        sx = SX(birdX[k] >> 4) - 8;
        sy = SY(birdY[k] >> 4) - 10;
        if (birdSt[k] == 0)
            townPut(i, sx, sy, (birdHx[k] >> 8) & 1, townTile[TOWN_SIT]);
        else
            townPut(i, sx, sy, 0, townTile[TOWN_FLY + ((frameCount >> 2) & 1)]);
    }
}
