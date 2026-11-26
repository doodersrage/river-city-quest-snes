/*---------------------------------------------------------------------------------
    Stages 5-6: dungeons. Room state (cleared rooms, locked doors, gates, cracked walls, chests
    that appear, boss stairs), small keys, the items on B (grappling hook, blasting powder),
    projectiles, keystones and the bosses (the Iron Foreman, Old Snapper).
    Ported from the web game's world.js (links, applyDyn), play.js (explode, updEnemy) and
    systems.js (updHook).
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
u8 hasHook, hasPowder, powder, eqItem; /* eqItem: ITEM_HOOK or ITEM_POWDER */
u8 keystones;  /* bit per keystone: granite, iron, brick, bronze */
u8 roomReward; /* keystone this boss room gives (1-4), 0 none */
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

/* bombs (blasting powder kegs) and the explosion */
#define MAX_BOMB 2
u8 bombOn[MAX_BOMB], bombT[MAX_BOMB];
s16 bombX[MAX_BOMB], bombY[MAX_BOMB];
u8 boomT;
s16 boomX, boomY;

/* the boss (1/16 px) */
u8 bossOn, bossType, bossHp, bossState, bossSt, bossFlash, bossFlip;
s16 bossX, bossY, bossVX, bossVY;
u16 bossT;
const u16 *bossTiles;
#define B_WALK 0
#define B_WIND 1
#define B_STUN 2
#define B_CHARGE 3
#define BOSS_FOREMAN 1
#define BOSS_TURTLE 2

extern const s8 dirX[4], dirY[4];

void dungeonInit(void)
{
    u8 i;
    for (i = 0; i < MAP_COUNT; i++) roomFlags[i] = 0;
    for (i = 0; i < sizeof(linkOpen); i++) linkOpen[i] = 0;
    for (i = 0; i < AREA_COUNT; i++) keys[i] = 0;
    hasHook = 0;
    hasPowder = 0;
    powder = 0;
    eqItem = 0;
    keystones = 0;
    dentMap = 255;
    boomT = 0;
    for (i = 0; i < MAX_BOMB; i++) bombOn[i] = 0;
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
                r = g + 4 + c * 10;
                if (colGrid[r[1] * COL_STRIDE + r[0]] != g[3])
                {
                    colGrid[r[1] * COL_STRIDE + r[0]] = g[3];
                    queuePatch(r[0], r[1], r + 2, now);
                }
            }
        g += 4 + n * 10;
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
                r = g + 4 + c * 10;
                if (r[0] == tx && r[1] == ty)
                {
                    linkOpen[g[1] >> 3] |= 1 << (g[1] & 7);
                    keys[roomArea]--;
                    applyRoomState(0);
                    return 1;
                }
            }
        g += 4 + n * 10;
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

/* ---------- bosses ---------- */

/* walls for a big body: a box of half-size h */
u8 bigBlocked(s16 x, s16 y, s16 h)
{
    if (x - h < 0 || y - h < 0 || x + h >= MAP_PX_W || y + h >= MAP_PX_H) return 1;
    if (solidAt(x - h, y - h) || solidAt(x + h, y - h) || solidAt(x - h, y + h) || solidAt(x + h, y + h)) return 1;
    if (solidAt(x, y - h) || solidAt(x, y + h) || solidAt(x - h, y) || solidAt(x + h, y)) return 1;
    return 0;
}

void bossLoad(void)
{
    bossOn = 0;
    bossType = roomBoss;
    if (!roomBoss || (roomFlags[curMap] & ROOM_BOSS)) return;
    bossOn = 1;
    bossTiles = bossType == BOSS_TURTLE ? boss2Tile : boss1Tile;
    bossHp = bossType == BOSS_TURTLE ? 8 : 9;
    bossX = (8 * 16 + 8) * 16;
    bossY = (4 * 16 + 8) * 16;
    if (bossType == BOSS_TURTLE) bossX = (9 * 16 + 8) * 16;
    bossState = B_WALK;
    bossSt = 0;
    bossT = 0;
    bossFlash = 0;
    bossFlip = 0;
    bossVX = 0;
    bossVY = 0;
}

void bossDefeated(void)
{
    u8 i;
    bossOn = 0;
    roomFlags[curMap] |= ROOM_BOSS | ROOM_CLEARED;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    if (!(roomFlags[curMap] & ROOM_CONTAINER))
        dropKind(144, 80, 3); /* the heart container */
    if (roomReward && !(keystones & (1 << (roomReward - 1))))
        dropKind(112, 80, 4 + roomReward); /* the keystone (kinds 5-8) */
    applyRoomState(0);
    openTextAt((u8 *)&sys_txt + bossOutTxt[bossType]);
}

/* Something hit the boss. src: 0 paddle, 1 bomb. Returns 1 for a hit, 2 for a clank off the armour. */
u8 bossHit(u8 dmg, u8 src)
{
    if (!bossOn || bossFlash) return 0;
    /* the Foreman only takes hits once the hook rips his plate off; Old Snapper takes bombs any
       time, and the paddle once he's stunned himself charging into a wall */
    if (bossState != B_STUN && !(bossType == BOSS_TURTLE && src == 1)) return 2;
    bossFlash = 16;
    if (bossHp <= dmg)
    {
        bossDefeated();
        return 1;
    }
    bossHp -= dmg;
    return 1;
}

void updForeman(s16 dx, s16 dy, s16 d)
{
    u8 i, a;
    s16 bx = bossX >> 4, by = bossY >> 4;
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
        return;
    }
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

/* Old Snapper (web updEnemy 'turtle'): plods toward you, winds up, charges in a straight line,
   and stuns himself on the wall. */
void updTurtle(s16 dx, s16 dy)
{
    s16 nx, ny;
    if (bossState == B_WIND)
    {
        if (!--bossSt)
        {
            bossState = B_CHARGE;
            bossVX = 0;
            bossVY = 0;
            if (ABS(dx) > ABS(dy)) bossVX = dx > 0 ? 42 : -42;
            else bossVY = dy > 0 ? 42 : -42;
        }
        return;
    }
    if (bossState == B_CHARGE)
    {
        nx = bossX + bossVX;
        ny = bossY + bossVY;
        if (bigBlocked(nx >> 4, ny >> 4, 10))
        {
            bossState = B_STUN;
            bossSt = 140;
            return;
        }
        bossX = nx;
        bossY = ny;
        return;
    }
    bossT++;
    if (bossT % 40 < 30)
    {
        if (ABS(dx) > ABS(dy)) nx = bossX + (dx > 0 ? 8 : -8), ny = bossY;
        else nx = bossX, ny = bossY + (dy > 0 ? 8 : -8);
        if (!bigBlocked(nx >> 4, ny >> 4, 10))
        {
            bossX = nx;
            bossY = ny;
        }
    }
    if (bossT >= 100)
    {
        bossState = B_WIND;
        bossSt = 36;
        bossT = 0;
    }
}

void updBoss(void)
{
    s16 dx, dy, d, bx, by;
    if (!bossOn) return;
    if (bossFlash) bossFlash--;
    bx = bossX >> 4;
    by = bossY >> 4;
    dx = px - bx;
    dy = py - by;
    d = ABS(dx) > ABS(dy) ? ABS(dx) + (ABS(dy) >> 1) : ABS(dy) + (ABS(dx) >> 1);
    if (!d) d = 1;
    if (bossState != B_CHARGE) bossFlip = dx < 0;
    if (bossState == B_STUN)
    {
        if (!--bossSt)
        {
            bossState = B_WALK;
            bossT = 0;
        }
        return; /* stunned bosses don't hurt */
    }
    if (bossType == BOSS_TURTLE) updTurtle(dx, dy);
    else updForeman(dx, dy, d);
    bx = bossX >> 4;
    by = bossY >> 4;
    if (ABS(bx - px) < 13 && ABS(by - py) < 13 && !dying)
        hurtHeroBy(bx, by, bossState == B_CHARGE ? 2 : 1);
}

void drawBoss(void)
{
    u8 k, f, col;
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
        if (bossType == BOSS_TURTLE)
            f = bossState == B_STUN ? 2 + ((frameCount >> 2) & 1) : (frameCount >> (bossState == B_CHARGE ? 1 : 3)) & 1;
        else
            f = bossState == B_STUN ? 3 + ((frameCount >> 2) & 1) : bossState == B_WIND ? 2 : (bossT >> 3) & 1;
        t = bossTiles[f * 9 + k];
        sh = bossState == B_WIND ? ((frameCount >> 1) & 1) * 2 - 1 : 0;
        /* the turtle's frames face right: mirror the 3x3 grid to face left */
        col = k % 3;
        if (bossType == BOSS_TURTLE && bossFlip) col = 2 - col;
        sx = (bossX >> 4) - camX - 24 + col * 16 + sh;
        sy = (bossY >> 4) - camY - 40 + (k / 3) * 16;
        if (!t || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, bossType == BOSS_TURTLE && bossFlip, 0, t, PAL_BOSS);
    }
}

/* ---------- blasting powder (web useItem 'powder' and explode) ---------- */

void dropBomb(void)
{
    u8 i;
    if (!powder)
    {
        openTextAt((u8 *)&sys_txt + TXT_NOPOWDER);
        return;
    }
    for (i = 0; i < MAX_BOMB; i++)
        if (!bombOn[i])
        {
            bombOn[i] = 1;
            bombT[i] = 80;
            bombX[i] = px + dirX[heroDir] * 12;
            bombY[i] = py + dirY[heroDir] * 12;
            powder--;
            return;
        }
}

void explode(s16 x, s16 y)
{
    u8 k, n, c, opened = 0;
    u8 *g;
    u8 *r;
    s16 dx, dy;
    boomT = 16;
    boomX = x;
    boomY = y;
    bombHitsEnemies(x, y);
    if (bossOn && ABS((bossX >> 4) - x) < 30 && ABS((bossY >> 4) - y) < 30) bossHit(2, 1);
    for (dy = -24; dy <= 24; dy += 16)
        for (dx = -24; dx <= 24; dx += 16)
            cutAt(x + dx, y + dy);
    if (ABS(px - x) < 20 && ABS(py - y) < 20) hurtHeroBy(x, y, 1);
    /* cracked walls and rubble within 32px give way */
    g = dynPtr + 1;
    for (k = 0; k < dynPtr[0]; k++)
    {
        n = g[2];
        if (g[0] == 2 && !groupOpen(g))
            for (c = 0; c < n; c++)
            {
                r = g + 4 + c * 10;
                if (ABS(r[0] * 16 + 8 - x) < 32 && ABS(r[1] * 16 + 8 - y) < 32)
                {
                    linkOpen[g[1] >> 3] |= 1 << (g[1] & 7);
                    opened = 1;
                    break;
                }
            }
        g += 4 + n * 10;
    }
    if (opened) applyRoomState(0);
}

void updBombs(void)
{
    u8 i;
    if (boomT) boomT--;
    for (i = 0; i < MAX_BOMB; i++)
        if (bombOn[i] && !--bombT[i])
        {
            bombOn[i] = 0;
            explode(bombX[i], bombY[i]);
        }
}

void drawBombs(void)
{
    u8 i, k;
    u16 id;
    s16 sx, sy;
    for (i = 0; i < MAX_BOMB; i++)
    {
        id = (OAM_BOMBS + i) * 4;
        sx = bombX[i] - camX - 8;
        sy = bombY[i] - camY - 10;
        if (!bombOn[i] || (bombT[i] < 24 && (frameCount & 2)))
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, 0, 0, OBJ_I_KEG, PAL_ITEM);
    }
    for (k = 0; k < 4; k++)
    {
        id = (OAM_BOOM + k) * 4;
        if (!boomT)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        sx = boomX - camX - 16 + (k & 1) * 16;
        sy = boomY - camY - 16 + (k >> 1) * 16;
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 3, 0, 0, OBJ_I_BOOM + ((boomT < 8 ? 4 : 0) + k) * 2, PAL_ITEM);
    }
}

/* ---------- items on B ---------- */

void useItem(void)
{
    if (eqItem == ITEM_HOOK) fireHook();
    else if (eqItem == ITEM_POWDER) dropBomb();
}

/* SELECT swaps between the items you have (the web game picks them in its menu) */
void nextItem(void)
{
    if (eqItem == ITEM_HOOK && hasPowder) eqItem = ITEM_POWDER;
    else if (eqItem == ITEM_POWDER && hasHook) eqItem = ITEM_HOOK;
}

void gotItem(u8 item)
{
    if (item == ITEM_HOOK) hasHook = 1;
    if (item == ITEM_POWDER)
    {
        hasPowder = 1;
        if (powder < 8) powder = 8; /* web give('powder') */
    }
    eqItem = item;
}

void gotKeystone(u8 id)
{
    u8 n = 0, b;
    keystones |= 1 << id;
    for (b = 0; b < 4; b++)
        if (keystones & (1 << b)) n++;
    openTextAt((u8 *)&sys_txt + keystoneTxt[id * 4 + n - 1]);
}

void drawItemHud(void)
{
    u16 id = OAM_BICON * 4;
    if (!eqItem)
    {
        oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
        oamSetEx((OAM_BICON + 1) * 4, OBJ_SMALL, OBJ_HIDE);
        oamSetEx((OAM_BICON + 2) * 4, OBJ_SMALL, OBJ_HIDE);
        return;
    }
    oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
    oamSet(id, 120, 4, 3, 0, 0, eqItem == ITEM_HOOK ? OBJ_I_HOOK : OBJ_I_KEG, PAL_ITEM);
    if (eqItem == ITEM_POWDER)
    {
        oamSetEx((OAM_BICON + 1) * 4, OBJ_SMALL, powder >= 10 ? OBJ_SHOW : OBJ_HIDE);
        oamSet((OAM_BICON + 1) * 4, 137, 12, 3, 0, 0, HUD_DIGIT + powder / 10, PAL_HUD);
        oamSetEx((OAM_BICON + 2) * 4, OBJ_SMALL, OBJ_SHOW);
        oamSet((OAM_BICON + 2) * 4, 145, 12, 3, 0, 0, HUD_DIGIT + powder % 10, PAL_HUD);
    }
    else
    {
        oamSetEx((OAM_BICON + 1) * 4, OBJ_SMALL, OBJ_HIDE);
        oamSetEx((OAM_BICON + 2) * 4, OBJ_SMALL, OBJ_HIDE);
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
            if (bossType == BOSS_FOREMAN && bossState != B_STUN)
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
    updBombs();
    updBoss();
    updShots();
    checkCleared();
}

void dungeonDraw(void)
{
    drawBoss();
    drawBombs();
    drawItemHud();
    drawHook();
    drawShots();
}

/* Called after a map is loaded (forced blank). */
void dungeonLoad(void)
{
    u8 i;
    hookSt = 0;
    boomT = 0;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    for (i = 0; i < MAX_BOMB; i++) bombOn[i] = 0;
    applyRoomState(1);
    bossLoad();
    if (roomBoss == BOSS_FOREMAN)
    {
        /* boss art into the second OBJ name table after the items (tile 320), palette 1 */
        dmaCopyVram(&boss1_chr, VRAM_OBJ_CHR + 0x1000 + 64 * 16, (&boss1_chrend - &boss1_chr));
        dmaCopyCGram(&boss1_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_TURTLE)
    {
        dmaCopyVram(&boss2_chr, VRAM_OBJ_CHR + 0x1000 + 64 * 16, (&boss2_chrend - &boss2_chr));
        dmaCopyCGram(&boss2_pal, 128 + 16, 16 * 2);
    }
}

/* the intro, once, when you first walk into a boss room */
void dungeonEntered(void)
{
    if (bossOn && !(roomFlags[curMap] & ROOM_SEEN))
    {
        roomFlags[curMap] |= ROOM_SEEN;
        openTextAt((u8 *)&sys_txt + bossInTxt[bossType]);
    }
}
