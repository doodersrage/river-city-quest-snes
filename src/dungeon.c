/*---------------------------------------------------------------------------------
    Stage 5: dungeons. Room state (cleared rooms, locked doors, gates, chests that appear,
    boss stairs), small keys, the grappling hook, projectiles and the Iron Foreman.
    Ported from the web game's world.js (links, applyDyn), play.js and systems.js (updHook).
---------------------------------------------------------------------------------*/
#include "game.h"

/* room data for the current map (set by main.c from the map's xtr blob) */
u8 roomArea, roomSolve, roomBoss, roomEntry;
u8 *dynPtr;    /* count, then groups: kind, ref, ncells, cells (tx, ty, 4 words) */
u8 *stairsPtr; /* count, then cells (tx, ty, 4 words) */

/* saved progress */
u8 roomFlags[MAP_COUNT]; /* ROOM_* bits */
u8 linkOpen[(LINK_COUNT + 7) / 8];
u8 keys[AREA_COUNT];
u8 hasHook;
u8 dentMap; /* dungeon entrance, for waking up after the fog takes you inside */

/* the hook */
u8 hookSt; /* 0 none, 1 out, 2 back, 3 pull */
s16 hookD, hookX, hookY, hookTX, hookTY;
u8 hookDir;

/* projectiles (1/16 px) */
#define MAX_SHOT 10
u8 shotOn[MAX_SHOT];
s16 shotX[MAX_SHOT], shotY[MAX_SHOT], shotVX[MAX_SHOT], shotVY[MAX_SHOT];
u8 shotLife[MAX_SHOT];

/* the boss (1/16 px) */
u8 bossOn, bossHp, bossState, bossSt, bossFlash;
s16 bossX, bossY;
u16 bossT;
#define B_WALK 0
#define B_WIND 1
#define B_STUN 2

extern const s8 dirX[4], dirY[4];

void dungeonInit(void)
{
    u8 i;
    for (i = 0; i < MAP_COUNT; i++) roomFlags[i] = 0;
    for (i = 0; i < sizeof(linkOpen); i++) linkOpen[i] = 0;
    for (i = 0; i < AREA_COUNT; i++) keys[i] = 0;
    hasHook = 0;
    dentMap = 255;
    hookSt = 0;
    bossOn = 0;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
}

u8 roomSolved(u8 m)
{
    return roomFlags[m] & ROOM_CLEARED;
}

u8 groupOpen(u8 *g)
{
    if (g[0] == 3) return roomSolved(g[1]);
    return linkOpen[g[1] >> 3] & (1 << (g[1] & 7));
}

/* Make the tilemap and collision match the room's saved state (web applyDyn). */
void applyRoomState(u8 now)
{
    u8 k, n, c, gid;
    u8 *g;
    u8 *r;
    g = dynPtr + 1;
    for (k = 0; k < dynPtr[0]; k++)
    {
        n = g[2];
        if (groupOpen(g))
            for (c = 0; c < n; c++)
            {
                r = g + 3 + c * 10;
                if (colGrid[r[1] * COL_STRIDE + r[0]] != CELL_OPEN)
                {
                    colGrid[r[1] * COL_STRIDE + r[0]] = CELL_OPEN;
                    queuePatch(r[0], r[1], r + 2, now);
                }
            }
        g += 3 + n * 10;
    }
    for (k = 0; k < chests[0]; k++)
    {
        r = chestRec(k);
        gid = r[2];
        if (chestOpen[gid >> 3] & (1 << (gid & 7)))
        {
            if (now) queuePatch(r[0], r[1], r + 15, now); /* opened (live opens patch themselves) */
            colGrid[r[1] * COL_STRIDE + r[0]] = CELL_SOLID;
        }
        else if (r[6] && roomSolved(curMap) && colGrid[r[1] * COL_STRIDE + r[0]] != CELL_SOLID)
        {
            queuePatch(r[0], r[1], r + 7, now); /* a chest appears */
            colGrid[r[1] * COL_STRIDE + r[0]] = CELL_SOLID;
        }
    }
    if (roomFlags[curMap] & ROOM_BOSS)
        for (k = 0; k < stairsPtr[0]; k++)
        {
            r = stairsPtr + 1 + k * 10;
            if (colGrid[r[1] * COL_STRIDE + r[0]] != CELL_EXIT)
            {
                colGrid[r[1] * COL_STRIDE + r[0]] = CELL_EXIT;
                queuePatch(r[0], r[1], r + 2, now);
            }
        }
}

/* Walking into a locked door with a small key opens it (web tryUnlock). */
u8 tryUnlock(s16 x, s16 y)
{
    u8 k, n, c, tx, ty;
    u8 *g;
    u8 *r;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H) return 0;
    tx = x >> 4;
    ty = y >> 4;
    if (colGrid[ty * COL_STRIDE + tx] != CELL_LOCK) return 0;
    if (!keys[roomArea]) return 2;
    g = dynPtr + 1;
    for (k = 0; k < dynPtr[0]; k++)
    {
        n = g[2];
        if (g[0] == 1)
            for (c = 0; c < n; c++)
            {
                r = g + 3 + c * 10;
                if (r[0] == tx && r[1] == ty)
                {
                    linkOpen[g[1] >> 3] |= 1 << (g[1] & 7);
                    keys[roomArea]--;
                    applyRoomState(0);
                    return 1;
                }
            }
        g += 3 + n * 10;
    }
    return 0;
}

/* After every enemy in a room is beaten, it stays beaten (web: clr flag). */
void checkCleared(void)
{
    if (mapW == 32) return; /* outdoors, enemies always come back */
    if (roomBoss || (roomFlags[curMap] & ROOM_CLEARED) || !enemiesExisted || enemiesAlive()) return;
    roomFlags[curMap] |= ROOM_CLEARED;
    applyRoomState(0);
}

/* ---------- projectiles ---------- */

void shoot(s16 x, s16 y, s16 vx, s16 vy)
{
    u8 i;
    for (i = 0; i < MAX_SHOT; i++)
        if (!shotOn[i])
        {
            shotOn[i] = 1;
            shotX[i] = x;
            shotY[i] = y;
            shotVX[i] = vx;
            shotVY[i] = vy;
            shotLife[i] = 200;
            return;
        }
}

void updShots(void)
{
    u8 i;
    s16 sx, sy;
    for (i = 0; i < MAX_SHOT; i++)
        if (shotOn[i])
        {
            shotX[i] += shotVX[i];
            shotY[i] += shotVY[i];
            sx = shotX[i] >> 4;
            sy = shotY[i] >> 4;
            if (!--shotLife[i] || sx < 0 || sy < 0 || sx >= MAP_PX_W || sy >= MAP_PX_H)
            {
                shotOn[i] = 0;
                continue;
            }
            if (ABS(sx - px) < 6 && ABS(sy - py) < 7 && !dying)
            {
                shotOn[i] = 0;
                hurtHero(sx, sy);
            }
        }
}

/* ---------- the Iron Foreman ---------- */

void bossLoad(void)
{
    bossOn = 0;
    if (roomBoss != 1 || (roomFlags[curMap] & ROOM_BOSS)) return;
    bossOn = 1;
    bossHp = 9;
    bossX = (8 * 16 + 8) * 16;
    bossY = (4 * 16 + 8) * 16;
    bossState = B_WALK;
    bossSt = 0;
    bossT = 0;
    bossFlash = 0;
}

void bossDefeated(void)
{
    u8 i;
    bossOn = 0;
    roomFlags[curMap] |= ROOM_BOSS | ROOM_CLEARED;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    if (!(roomFlags[curMap] & ROOM_CONTAINER))
        dropKind(144, 80, 3); /* the heart container */
    applyRoomState(0);
    openTextAt((u8 *)&sys_txt + TXT_FOREMAN_OUT);
}

/* The paddle or spin reached the boss. Returns 1 for a hit, 2 for a clank off the armour. */
u8 bossHit(u8 dmg)
{
    if (!bossOn || bossFlash) return 0;
    if (bossState != B_STUN) return 2;
    bossFlash = 16;
    if (bossHp <= dmg)
    {
        bossDefeated();
        return 1;
    }
    bossHp -= dmg;
    return 1;
}

void updBoss(void)
{
    s16 dx, dy, d, bx, by;
    u8 i, a;
    if (!bossOn) return;
    if (bossFlash) bossFlash--;
    bx = bossX >> 4;
    by = bossY >> 4;
    dx = px - bx;
    dy = py - by;
    d = ABS(dx) > ABS(dy) ? ABS(dx) + (ABS(dy) >> 1) : ABS(dy) + (ABS(dx) >> 1);
    if (!d) d = 1;
    if (bossState == B_STUN)
    {
        if (!--bossSt)
        {
            bossState = B_WALK;
            bossT = 0;
        }
        return;
    }
    if (bossState == B_WIND)
    {
        if (!--bossSt)
        {
            bossState = B_WALK;
            for (i = 0; i < 8; i++)
            {
                a = i * 8 + (bossHp < 5 ? 4 : 0);
                shoot(bossX, bossY, sinTab[(a + 16) & 63] * 24 / 64, sinTab[a & 63] * 24 / 64);
            }
        }
    }
    else
    {
        if (d > 4)
        {
            if (!enBlocked((bossX + dx * 7 / d) >> 4, by)) bossX += dx * 7 / d;
            if (!enBlocked(bx, (bossY + dy * 7 / d) >> 4)) bossY += dy * 7 / d;
        }
        bossT++;
        if (bossT % 150 == 149)
        {
            bossState = B_WIND;
            bossSt = 40;
        }
        if (bossHp <= 5 && bossT % 90 == 45)
            shoot(bossX, bossY, dx * 27 / d, dy * 27 / d);
    }
    if (ABS(bx - px) < 13 && ABS(by - py) < 13 && !dying)
        hurtHero(bx, by);
}

void drawBoss(void)
{
    u8 k, f;
    u16 id, t;
    s16 sx, sy, sh;
    for (k = 0; k < 9; k++)
    {
        id = (OAM_BOSS + k) * 4;
        if (!bossOn || (bossFlash & 2))
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        f = bossState == B_STUN ? 3 + ((frameCount >> 2) & 1) : bossState == B_WIND ? 2 : (bossT >> 3) & 1;
        t = foremanTile[f * 9 + k];
        sh = bossState == B_WIND ? ((frameCount >> 1) & 1) * 2 - 1 : 0;
        sx = (bossX >> 4) - camX - 24 + (k % 3) * 16 + sh;
        sy = (bossY >> 4) - camY - 40 + (k / 3) * 16;
        if (!t || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, 0, 0, t, PAL_BOSS);
    }
}

/* ---------- the grappling hook (web updHook) ---------- */

void fireHook(void)
{
    if (!hasHook || hookSt) return;
    hookSt = 1;
    hookD = 0;
    hookDir = heroDir;
    hookX = px;
    hookY = py - 4;
}

void updHook(void)
{
    u8 k, c;
    s16 tx, ty, dx, dy, l;
    if (!hookSt) return;
    if (hookSt == 1)
    {
        hookD += 5;
        hookX = px + dirX[hookDir] * hookD;
        hookY = py - 4 + dirY[hookDir] * hookD;
        if (bossOn && ABS((bossX >> 4) - hookX) < 15 && ABS((bossY >> 4) - 6 - hookY) < 18)
        {
            if (bossState != B_STUN)
            {
                bossState = B_STUN; /* the chest plate rips loose */
                bossSt = 170;
                bossFlash = 8;
            }
            hookSt = 2;
            return;
        }
        if (hookHitsEnemy(hookX, hookY, dirX[hookDir] * 32, dirY[hookDir] * 32))
        {
            hookSt = 2;
            return;
        }
        hookGrabsPickup(hookX, hookY + 4);
        if (hookX < 0 || hookY + 4 < 0 || hookX >= MAP_PX_W || hookY + 4 >= MAP_PX_H || hookD >= 104)
        {
            hookSt = 2;
            return;
        }
        tx = hookX >> 4;
        ty = (hookY + 4) >> 4;
        c = colGrid[ty * COL_STRIDE + tx];
        if (c == CELL_POST)
        {
            hookSt = 3; /* fly to the cell in front of the post */
            hookX = tx * 16 + 8;
            hookY = ty * 16 + 4;
            hookTX = (tx - dirX[hookDir]) * 16 + 8;
            hookTY = (ty - dirY[hookDir]) * 16 + 8;
            return;
        }
        if (c == CELL_SOLID || c == CELL_LOCK || c == CELL_COUNTER || c == CELL_BUSH)
            hookSt = 2;
    }
    else if (hookSt == 2)
    {
        hookD -= 7;
        hookX = px + dirX[hookDir] * hookD;
        hookY = py - 4 + dirY[hookDir] * hookD;
        if (hookD <= 0) hookSt = 0;
    }
    else
    {
        dx = hookTX - px;
        dy = hookTY - py;
        l = ABS(dx) + ABS(dy);
        if (inv < 2) inv = 2;
        if (l <= 4)
        {
            px = hookTX;
            py = hookTY;
            hookSt = 0;
            return;
        }
        px += dx > 0 ? (dx > 4 ? 4 : dx) : (dx < -4 ? -4 : dx);
        py += dy > 0 ? (dy > 4 ? 4 : dy) : (dy < -4 ? -4 : dy);
    }
}

void drawHook(void)
{
    u8 k;
    u16 id;
    s16 d, sx, sy;
    for (k = 0; k < 14; k++)
    {
        id = (OAM_HOOK + k) * 4;
        if (!hookSt)
        {
            oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
            continue;
        }
        if (k == 0)
        {
            sx = hookX - camX - 4;
            sy = hookY - camY - 4;
        }
        else
        {
            /* chain links every 8px from the hero to the head */
            d = k * 8;
            if (hookSt == 3) /* pulling: the chain runs from the hero to the post */
            {
                sx = px + (hookX - px) * k / 14;
                sy = py - 4 + (hookY - py + 4) * k / 14;
                sx -= camX + 4;
                sy -= camY + 4;
            }
            else
            {
                if (d >= hookD)
                {
                    oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
                    continue;
                }
                sx = px + dirX[hookDir] * d - camX - 4;
                sy = py - 4 + dirY[hookDir] * d - camY - 4;
            }
        }
        oamSetEx(id, OBJ_SMALL, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 3, 0, 0, k ? OBJ_CHAIN : OBJ_HOOK, PAL_HUD);
    }
}

void drawShots(void)
{
    u8 i;
    u16 id;
    s16 sx, sy;
    for (i = 0; i < MAX_SHOT; i++)
    {
        id = (OAM_SHOTS + i) * 4;
        sx = (shotX[i] >> 4) - camX - 4;
        sy = (shotY[i] >> 4) - camY - 4;
        if (!shotOn[i] || sx < -8 || sx > SCREEN_W || sy < -8 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
            continue;
        }
        oamSetEx(id, OBJ_SMALL, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 3, 0, 0, OBJ_SPARK + ((frameCount >> 1) & 1), PAL_HUD);
    }
}

void dungeonUpdate(void)
{
    updHook();
    updBoss();
    updShots();
    checkCleared();
}

void dungeonDraw(void)
{
    drawBoss();
    drawHook();
    drawShots();
}

/* Called after a map is loaded (forced blank). */
void dungeonLoad(void)
{
    u8 i;
    hookSt = 0;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    applyRoomState(1);
    bossLoad();
    if (roomBoss)
    {
        /* boss art into the second OBJ name table, palettes 1-2 (no townspeople down here) */
        dmaCopyVram(&boss_chr, VRAM_OBJ_CHR + 0x1000, (&boss_chrend - &boss_chr));
        dmaCopyCGram(&boss_pal, 128 + 16, 2 * 16 * 2);
    }
}

/* the intro, once, when you first walk into a boss room */
void dungeonEntered(void)
{
    if (bossOn && !(roomFlags[curMap] & ROOM_SEEN))
    {
        roomFlags[curMap] |= ROOM_SEEN;
        openTextAt((u8 *)&sys_txt + TXT_FOREMAN_IN);
    }
}
