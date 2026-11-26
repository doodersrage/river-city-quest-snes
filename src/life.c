/* River City Quest (SNES): town life. Townspeople who wander about, cars and the Pulse bus that stop for you,
   and pigeons that take off when you come near (web updLife, drawCar). */
#include <snes.h>
#include "game.h"

#define OAM_CARS 112 /* 3 per car: 112-117 */
#define OAM_BIRDS 118
#define MAX_CARS 2
#define MAX_BIRDS 2

/* wandering townspeople: home spot, a step every few frames, a new idea every 50-140 frames */
s16 npcHx[MAX_NPCS], npcHy[MAX_NPCS];
s8 npcVx[MAX_NPCS], npcVy[MAX_NPCS];
u8 npcT[MAX_NPCS];

/* cars: x in 1/16 px, lane y in px; kind 0 car, 1 bus */
u8 carOn[MAX_CARS], carBus[MAX_CARS], carCol[MAX_CARS], carStop[MAX_CARS];
s8 carDir[MAX_CARS];
s16 carX[MAX_CARS], carY[MAX_CARS];

/* pigeons: 0 sitting, 1 flying, 2 gone, 255 none; x, y in 1/16 px */
u8 birdSt[MAX_BIRDS];
u16 birdT[MAX_BIRDS];
s16 birdX[MAX_BIRDS], birdY[MAX_BIRDS], birdVx[MAX_BIRDS], birdVy[MAX_BIRDS];
s16 birdHx[MAX_BIRDS], birdHy[MAX_BIRDS];

static u8 carHalf(u8 k)
{
    return carBus[k] ? 20 : 13;
}

/* web carHit: a car or the bus fills its lane this wide */
u8 carHit(s16 x, s16 y)
{
    u8 k;
    s16 cx;
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
    for (k = 0; k < MAX_NPCS; k++)
    {
        npcHx[k] = npcX[k];
        npcHy[k] = npcY[k];
        npcVx[k] = 0;
        npcVy[k] = 0;
        npcT[k] = rnd() % 60;
    }
    for (k = 0; k < MAX_CARS; k++)
    {
        l = carLane[curMap * 2 + k];
        carOn[k] = l != 0;
        if (!l) continue;
        carBus[k] = l >> 7;
        carDir[k] = (l & 0x40) ? -1 : 1;
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
    }
    /* their art goes where a boss would load: outdoor maps never have one */
    if (!roomBoss && (carOn[0] || carOn[1] || birdSt[0] != 255 || birdSt[1] != 255))
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

void lifeUpdate(void)
{
    u8 k, a;
    s16 nx, ny, ah, dx, dy;
    u8 *rec;
    /* townspeople (web: 0.4 px a frame; here a pixel on 3 frames in 8) */
    if (!boxState)
        for (k = 0; k < npcCount; k++)
        {
            rec = wld + WLD_NPCS + k * NPC_REC;
            if (!(rec[4] & 0x80)) continue;
            if (npcT[k]) npcT[k]--;
            else
            {
                npcT[k] = 50 + rnd() % 90;
                a = rnd() % 6;
                npcVx[k] = a == 2 ? -1 : a == 3 ? 1 : 0;
                npcVy[k] = a == 0 ? 1 : a == 1 ? -1 : 0;
            }
            if ((!npcVx[k] && !npcVy[k]) || (frameCount & 7) >= 3) continue;
            nx = npcX[k] + npcVx[k];
            ny = npcY[k] + npcVy[k];
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
    /* cars stop for you when you're in the lane ahead (web: a short horn) */
    for (k = 0; k < MAX_CARS; k++)
    {
        if (!carOn[k]) continue;
        ah = (px - (carX[k] >> 4)) * carDir[k];
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
        carX[k] += carDir[k] * (carBus[k] ? 13 : 18);
        if ((carX[k] >> 4) > mapRight()) carX[k] = -40 << 4;
        if ((carX[k] >> 4) < -40) carX[k] = mapRight() << 4;
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
            if (ABS(dx) < 30 && ABS(dy) < 30 && dx * dx + dy * dy < 900)
            {
                birdSt[k] = 1;
                birdT[k] = 0;
                birdVx[k] = (dx > 0 ? -1 : 1) * (16 + (rnd() & 15));
                birdVy[k] = -26 - (rnd() & 15);
                sfx(SFX_FLAP);
            }
        }
        else if (birdSt[k] == 1)
        {
            birdX[k] += birdVx[k];
            birdY[k] += birdVy[k];
            if (birdT[k] % 3 == 0) birdVy[k]--;
            if ((birdY[k] >> 4) < camY - 20 || birdT[k] > 300)
            {
                birdSt[k] = 2;
                birdT[k] = 0;
            }
        }
        else if (birdT[k] > 600 && (ABS(px - (birdHx[k] >> 4)) > 120 || ABS(py - (birdHy[k] >> 4)) > 120))
        {
            birdSt[k] = 0;
            birdX[k] = birdHx[k];
            birdY[k] = birdHy[k];
        }
    }
}

void lifeDraw(void)
{
    u8 k, c, n, slot, flip;
    s16 sx, sy, x;
    u16 t;
    for (k = 0; k < MAX_CARS; k++)
    {
        n = carBus[k] ? 3 : 2;
        for (c = 0; c < 3; c++)
        {
            slot = OAM_CARS + k * 3 + c;
            if (!carOn[k] || c >= n)
            {
                sprHide(slot);
                continue;
            }
            /* drawn facing right; going left, mirror each cell and lay them out the other way */
            x = (carX[k] >> 4) - n * 8;
            flip = carDir[k] < 0;
            sx = x + (flip ? (n - 1 - c) : c) * 16 - camX;
            sy = carY[k] - 10 - camY;
            if (sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
            {
                sprHide(slot);
                continue;
            }
            t = townTile[carBus[k] ? TOWN_BUS + c : TOWN_CAR + carCol[k] * 2 + c];
            sprPut(slot, 1, sx, sy, 2, flip, t, PAL_ITEM);
        }
    }
    for (k = 0; k < MAX_BIRDS; k++)
    {
        slot = OAM_BIRDS + k;
        if (birdSt[k] > 1)
        {
            sprHide(slot);
            continue;
        }
        sx = (birdX[k] >> 4) - 8 - camX;
        sy = (birdY[k] >> 4) - 10 - camY;
        if (sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            sprHide(slot);
            continue;
        }
        if (birdSt[k] == 0)
            sprPut(slot, 1, sx, sy, 2, (birdHx[k] >> 8) & 1, townTile[TOWN_SIT], PAL_ITEM);
        else
            sprPut(slot, 1, sx, sy, 2, 0, townTile[TOWN_FLY + ((frameCount >> 1) & 1)], PAL_ITEM);
    }
}
