/* River City Quest (SNES): town life. Townspeople who wander about, cars and the Pulse bus that stop for you,
   and pigeons that take off when you come near (web updLife, drawCar). */
#include <snes.h>
#include "game.h"

#define OAM_CARS 112 /* 3 per car: 112-117 */
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
s16 lastSx[8], lastSy[8];
u16 lastTile[8];
u8 lastOn[8], lifeAny;

static void townHide(u8 i)
{
    if (!lastOn[i]) return;
    lastOn[i] = 0;
    sprHide(OAM_CARS + i);
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
    sprPut(OAM_CARS + i, 1, sx, sy, 2, flip, tile & 0x7FFF, PAL_ITEM);
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
    for (k = 0; k < 8; k++)
    {
        lastOn[k] = 1;
        townHide(k);
    }
    lifeAny = carAny || birdSt[0] != 255 || birdSt[1] != 255;
    /* their art goes where a boss would load: outdoor maps never have one */
    if (!roomBoss && (carAny || birdSt[0] != 255 || birdSt[1] != 255))
        dmaCopyVram(&town_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&town_chrend - &town_chr));
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
    for (k = 0; k < MAX_CARS; k++)
    {
        if (!carOn[k]) continue;
        cx = carX[k] >> 4;
        ah = carDir[k] > 0 ? px - cx : cx - px;
        if (ABS(py - carY[k]) < 13 && ah > -(s16)carHalf(k) - 6 && ah < (s16)carHalf(k) + 22)
        {
            if (!carStop[k])
            {
                carStop[k] = 1;
                sfx(SFX_HORN);
            }
            continue;
        }
        carStop[k] = 0;
        carX[k] += carSp[k] << 1;
        if (cx > mapRight()) carX[k] = -40 << 4;
        else if (cx < -40) carX[k] = mapRight() << 4;
    }
}

void lifeUpdate(void)
{
    u8 k, i, a;
    s16 nx, ny, dx, dy;
    /* odd frames: townspeople and pigeons; even frames: traffic (each moves twice as far) */
    if (!(frameCount & 1))
    {
        lifeTraffic();
        return;
    }
    /* townspeople (web: 0.4 px a frame). Timers tick for all; one wanderer takes a step. */
    if (nWander && !boxState)
    {
        for (i = 0; i < nWander; i++)
        {
            k = wanderer[i];
            if (npcT[k]) npcT[k]--;
            else
            {
                npcT[k] = 25 + rnd() % 45;
                a = rnd() % 6;
                npcVx[k] = a == 2 ? -1 : a == 3 ? 1 : 0;
                npcVy[k] = a == 0 ? 1 : a == 1 ? -1 : 0;
            }
        }
        if (++wTurn >= nWander) wTurn = 0;
        k = wanderer[wTurn];
        if (npcVx[k] || npcVy[k])
        {
            /* with two or three out walking, each gets a turn less often: take a bigger step */
            nx = npcX[k] + (nWander > 1 ? npcVx[k] << 1 : npcVx[k]);
            ny = npcY[k] + (nWander > 1 ? npcVy[k] << 1 : npcVy[k]);
            if (ABS(nx - npcHx[k]) < 40 && ABS(ny - npcHy[k]) < 40 && npcFree(nx, ny))
            {
                npcX[k] = nx;
                npcY[k] = ny;
                npcDir[k] = npcVx[k] < 0 ? 2 : npcVx[k] > 0 ? 3 : npcVy[k] < 0 ? 1 : 0;
            }
            else
            {
                npcVx[k] = 0;
                npcVy[k] = 0;
            }
        }
    }
    /* pigeons */
    for (k = 0; k < MAX_BIRDS; k++)
    {
        if (birdSt[k] == 255) continue;
        birdT[k]++;
        if (birdSt[k] == 0)
        {
            dx = px - (birdX[k] >> 4);
            dy = py - (birdY[k] >> 4);
            if (ABS(dx) < 30 && ABS(dy) < 30 && ABS(dx) + ABS(dy) < 40)
            {
                birdSt[k] = 1;
                birdT[k] = 0;
                birdV[k] = 0;
                birdVx[k] = 16 + (rnd() & 15);
                if (dx > 0) birdVx[k] = -birdVx[k];
                birdVy[k] = -26 - (rnd() & 15);
                sfx(SFX_FLAP);
            }
        }
        else if (birdSt[k] == 1)
        {
            birdX[k] += birdVx[k] << 1;
            birdY[k] += birdVy[k] << 1;
            if (++birdV[k] == 2) /* web: vy -= .02 a frame */
            {
                birdV[k] = 0;
                birdVy[k]--;
            }
            if ((birdY[k] >> 4) < camY - 20 || birdT[k] > 150)
            {
                birdSt[k] = 2;
                birdT[k] = 0;
            }
        }
        else if (birdT[k] > 300 && (ABS(px - (birdHx[k] >> 4)) > 120 || ABS(py - (birdHy[k] >> 4)) > 120))
        {
            birdSt[k] = 0;
            birdX[k] = birdHx[k];
            birdY[k] = birdHy[k];
        }
    }
}

void lifeDraw(void)
{
    u8 k, c, n, i, flip, base;
    s16 sx, sy;
    if (!lifeAny) return;
    for (k = 0; k < MAX_CARS; k++)
    {
        if (!carOn[k]) continue;
        /* drawn facing right; going left, mirror each cell and take them in the other order */
        n = carBus[k] ? 3 : 2;
        base = carBus[k] ? TOWN_BUS : TOWN_CAR + (carCol[k] << 1);
        flip = carDir[k] < 0;
        sx = (carX[k] >> 4) - (n << 3) - camX;
        sy = carY[k] - 10 - camY;
        i = k * 3;
        for (c = 0; c < n; c++, i++, sx += 16)
            townPut(i, sx, sy, flip, townTile[base + (flip ? n - 1 - c : c)]);
    }
    for (k = 0; k < MAX_BIRDS; k++)
    {
        i = 6 + k;
        if (birdSt[k] > 1)
        {
            townHide(i);
            continue;
        }
        sx = (birdX[k] >> 4) - 8 - camX;
        sy = (birdY[k] >> 4) - 10 - camY;
        if (birdSt[k] == 0)
            townPut(i, sx, sy, (birdHx[k] >> 8) & 1, townTile[TOWN_SIT]);
        else
            townPut(i, sx, sy, 0, townTile[TOWN_FLY + ((frameCount >> 2) & 1)]);
    }
}
