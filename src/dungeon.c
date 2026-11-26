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
u8 *brazPtr;   /* count, then braziers (tx, ty, 4 words lit) */
u8 roomBraz[MAP_COUNT]; /* lit braziers, a bit each */

/* saved progress */
u8 roomFlags[MAP_COUNT]; /* ROOM_* bits */
u8 linkOpen[(LINK_COUNT + 7) / 8];
u8 keys[AREA_COUNT];
u8 hasHook, hasPowder, hasLantern, hasShield, hasBug, hasCan, powder, eqItem;
u8 *pickPtr;   /* count, then pickups lying in the room: x, y (2 bytes each), kind, id */
u8 *blkPtr;    /* count, then push blocks (tx, ty); then count, then switches (tx, ty) */
u8 hasCarkey, pieces, hasKayak, tradeItem, melonTaken;
u8 parts, bateauDone, hasLure;

/* push blocks (web blocks, tryPush, updBlocks) */
#define MAX_BLK 4
u8 blkN, blkTX[MAX_BLK], blkTY[MAX_BLK], blkMv[MAX_BLK], pushN, pushDir;
s16 blkX[MAX_BLK], blkY[MAX_BLK], blkDX[MAX_BLK], blkDY[MAX_BLK];
s16 bossZ; /* the Bell's height */
u8 pickupGot[(PICKUP_COUNT + 7) / 8];

/* the Gold-Bug (web updBug), in pixels */
u8 bugOn, bugRet, bugT, bugCarry, bugHit;
s16 bugX, bugY, bugVX, bugVY; /* eqItem: ITEM_HOOK or ITEM_POWDER */
u8 keystones;  /* bit per keystone: granite, iron, brick, bronze */
u8 roomReward; /* keystone this boss room gives (1-4), 0 none */
u8 hudEq, hudPowder; /* what the item HUD last showed */
u8 dentMap; /* dungeon entrance, for waking up after the fog takes you inside */

/* the hook */
u8 hookSt; /* 0 none, 1 out, 2 back, 3 pull */
s16 hookD, hookX, hookY, hookTX, hookTY;
u8 hookDir;

/* projectiles (1/16 px) */
#define MAX_SHOT 10
u8 shotOn[MAX_SHOT], shotKind[MAX_SHOT], shotRefl[MAX_SHOT]; /* kind: 0 spark, 1 orb */
s16 shotX[MAX_SHOT], shotY[MAX_SHOT], shotVX[MAX_SHOT], shotVY[MAX_SHOT];
u8 shotLife[MAX_SHOT];

/* bombs (blasting powder kegs) and the explosion */
#define MAX_BOMB 2
u8 bombOn[MAX_BOMB], bombT[MAX_BOMB];
s16 bombX[MAX_BOMB], bombY[MAX_BOMB];
u8 boomT;
s16 boomX, boomY;

/* the boss (1/16 px) */
u8 bossVis; /* the Wraith: frames left solid after its own orb hits it */
u8 bossOn, bossType, bossHp, bossState, bossSt, bossFlash, bossFlip;
s16 bossX, bossY, bossVX, bossVY;
u8 vampP2, vampP3, vampSwarm, vampGone, gameWon, endingPending;
u16 bossT;
const u16 *bossTiles;
#define B_WALK 0
#define B_WIND 1
#define B_STUN 2
#define B_CHARGE 3
#define BOSS_FOREMAN 1
#define BOSS_TURTLE 2
#define BOSS_WRAITH 3
#define BOSS_RAVEN 4
#define BOSS_BELL 5
#define BOSS_ARUM 6
#define BOSS_VAMPIRE 7
#define T_BAT 5
#define B_DROP 4
#define B_RISE 5
#define SRC_PADDLE 0
#define SRC_BOMB 1
#define SRC_REFL 2
#define T_GHOST 6
#define T_RAT 0

extern const s8 dirX[4], dirY[4];

void dungeonInit(void)
{
    u8 i;
    for (i = 0; i < MAP_COUNT; i++)
    {
        roomFlags[i] = 0;
        roomBraz[i] = 0;
    }
    hasLantern = 0;
    hasShield = 0;
    hasBug = 0;
    hasCan = 0;
    hasCarkey = 0;
    pieces = 0;
    hasKayak = 0;
    tradeItem = 0;
    melonTaken = 0;
    parts = 0;
    bateauDone = 0;
    hasLure = 0;
    chalOn = 0;
    pins = 0;
    goldPaddle = 0;
    for (i = 0; i < (LORE_COUNT + 7) / 8; i++) loreRead[i] = 0;
    blkN = 0;
    bugOn = 0;
    for (i = 0; i < sizeof(pickupGot); i++) pickupGot[i] = 0;
    for (i = 0; i < sizeof(linkOpen); i++) linkOpen[i] = 0;
    for (i = 0; i < AREA_COUNT; i++) keys[i] = 0;
    hasHook = 0;
    hasPowder = 0;
    powder = 0;
    eqItem = 0;
    keystones = 0;
    hudEq = 255;
    hudPowder = 255;
    dentMap = 255;
    boomT = 0;
    for (i = 0; i < MAX_BOMB; i++) bombOn[i] = 0;
    hookSt = 0;
    bossOn = 0;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    gameWon = 0;
    endingPending = 0;
}

u8 roomSolved(u8 m)
{
    return roomFlags[m] & ROOM_SOLVED;
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
    for (k = 0; k < brazPtr[0]; k++)
    {
        r = brazPtr + 1 + k * 10;
        if (now && (roomBraz[curMap] & (1 << k)))
            queuePatch(r[0], r[1], r + 2, now);
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
    /* something appeared where you stand (a reward chest, a gate): step clear of it */
    if (!now && !hookSt && !dash) settle(); /* (the hook and a dash may carry you over pits) */
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
                    sfx(SFX_SECRET);
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
    if (roomSolve == 1 && !(roomFlags[curMap] & ROOM_SOLVED))
    {
        roomFlags[curMap] |= ROOM_SOLVED;
        sfx(SFX_SECRET);
    }
    applyRoomState(0);
}

/* ---------- projectiles ---------- */

u8 shotSfxAt;

void shoot(s16 x, s16 y, s16 vx, s16 vy, u8 kind)
{
    u8 i;
    if (shotSfxAt != (u8)frameCount)
    {
        shotSfxAt = frameCount; /* one sound per volley */
        sfx(SFX_SHOT);
    }
    for (i = 0; i < MAX_SHOT; i++)
        if (!shotOn[i])
        {
            shotOn[i] = 1;
            shotKind[i] = kind;
            shotRefl[i] = 0;
            shotX[i] = x;
            shotY[i] = y;
            shotVX[i] = vx;
            shotVY[i] = vy;
            shotLife[i] = 200;
            return;
        }
}

u8 bossHit(u8 dmg, u8 src);

/* Bat a shot back (web reflect): the Wraith's orbs fly straight at it, everything else reverses. */
void reflectShot(u8 i)
{
    s16 dx, dy, d;
    shotRefl[i] = 1;
    shotLife[i] = 160;
    if (shotKind[i] == 1 && bossOn && bossType == BOSS_WRAITH)
    {
        dx = (bossX - shotX[i]) >> 4;
        dy = (bossY - shotY[i]) >> 4;
        d = ABS(dx) > ABS(dy) ? ABS(dx) + (ABS(dy) >> 1) : ABS(dy) + (ABS(dx) >> 1);
        if (!d) d = 1;
        shotVX[i] = dx * 35 / d;
        shotVY[i] = dy * 35 / d;
    }
    else
    {
        shotVX[i] = -shotVX[i] * 13 / 10;
        shotVY[i] = -shotVY[i] * 13 / 10;
    }
}

void updShots(void)
{
    u8 i;
    s16 sx, sy, hx, hy;
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
            if (shotRefl[i])
            {
                /* a returned shot hits whatever it's sent at */
                if (bossOn && ABS((bossX >> 4) - sx) < 13 && ABS((bossY >> 4) - 4 - sy) < 14)
                {
                    shotOn[i] = 0;
                    bossHit(1, SRC_REFL);
                }
                else if (hookHitsEnemy(sx, sy - 4, shotVX[i], shotVY[i]))
                    shotOn[i] = 0;
                continue;
            }
            /* the paddle mid-swing, or a spin, knocks shots back */
            if (atk > 5)
            {
                hx = px + dirX[heroDir] * 12;
                hy = py - 2 + dirY[heroDir] * 12;
                if (ABS(sx - hx) < 11 && ABS(sy - hy) < 11)
                {
                    reflectShot(i);
                    continue;
                }
            }
            if (spin && ABS(sx - px) < 22 && ABS(sy - (py - 3)) < 22)
            {
                reflectShot(i);
                continue;
            }
            if (ABS(sx - px) < 7 && ABS(sy - (py - 4)) < 8 && !dying)
            {
                /* the Cast-Iron Shield turns shots you're facing */
                if (hasShield && !atk && !spin && dirX[heroDir] * shotVX[i] + dirY[heroDir] * shotVY[i] < 0)
                {
                    reflectShot(i);
                    continue;
                }
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
    bossVis = 0;
    /* hit points and starting cells from the web game (HPS, the rooms' en lists) */
    if (bossType == BOSS_TURTLE)
    {
        bossTiles = boss2Tile;
        bossHp = 8;
        bossX = (9 * 16 + 8) * 16;
        bossY = (4 * 16 + 8) * 16;
    }
    else if (bossType == BOSS_WRAITH)
    {
        bossTiles = boss3Tile;
        bossHp = 6;
        bossX = (8 * 16 + 8) * 16;
        bossY = (3 * 16 + 8) * 16;
    }
    else if (bossType == BOSS_RAVEN)
    {
        bossTiles = boss4Tile;
        bossHp = 7;
        bossX = (8 * 16 + 8) * 16;
        bossY = (2 * 16 + 8) * 16;
    }
    else if (bossType == BOSS_BELL)
    {
        bossTiles = boss5Tile;
        bossHp = 8;
        bossX = (8 * 16 + 8) * 16;
        bossY = (4 * 16 + 8) * 16;
        bossZ = 40;
    }
    else if (bossType == BOSS_VAMPIRE)
    {
        bossTiles = boss7Tile;
        bossHp = 14;
        bossX = (8 * 16 + 8) * 16;
        bossY = (5 * 16 + 8) * 16;
        vampP2 = 0;
        vampP3 = 0;
        vampSwarm = 0;
        vampGone = 0;
    }
    else if (bossType == BOSS_ARUM)
    {
        bossTiles = boss6Tile;
        bossHp = 8;
        bossX = (8 * 16 + 8) * 16;
        bossY = (3 * 16 + 8) * 16;
    }
    else
    {
        bossTiles = boss1Tile;
        bossHp = 9;
        bossX = (8 * 16 + 8) * 16;
        bossY = (4 * 16 + 8) * 16;
    }
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
    sfx(SFX_FANFARE);
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    for (i = 0; i < enCount; i++) enAlive[i] = 0;
    if (bossType == BOSS_VAMPIRE)
    {
        gameWon = 1;
        endingPending = 1;
    }
    else if (!(roomFlags[curMap] & ROOM_CONTAINER))
        dropKind(144, 80, 3); /* the heart container */
    if (roomReward && !(keystones & (1 << (roomReward - 1))))
        dropKind(112, 80, 4 + roomReward); /* the keystone (kinds 5-8) */
    applyRoomState(0);
    openTextAt((u8 *)&sys_txt + bossOutTxt[bossType]);
}

/* Something hit the boss. src: SRC_PADDLE, SRC_BOMB, SRC_REFL. Returns 1 for a hit, 2 for a clank. */
u8 bossHit(u8 dmg, u8 src)
{
    if (!bossOn || bossFlash) return 0;
    if (bossType == BOSS_WRAITH)
    {
        /* faint, it can't be touched, except by its own orb sent back (web hitEnemy 'wraith') */
        if (!bossVis)
        {
            if (src != SRC_REFL) return 0;
            bossVis = 130;
        }
        bossFlash = 16;
        if (bossHp <= dmg)
        {
            bossDefeated();
            return 1;
        }
        bossHp -= dmg;
        return 1;
    }
    /* the Foreman only takes hits once the hook rips his plate off; Old Snapper takes bombs any
       time, and the paddle once he's stunned himself charging into a wall */
    /* the Bell can't be reached up high, and only a keg dents it unless it has just slammed down */
    if (bossType == BOSS_BELL && bossZ > 12) return 0;
    if (bossType == BOSS_VAMPIRE && (vampGone || vampSwarm)) return 0;
    if (bossType != BOSS_RAVEN && bossType != BOSS_VAMPIRE && bossState != B_STUN && !((bossType == BOSS_TURTLE || bossType == BOSS_BELL) && src == SRC_BOMB))
    {
        sfx(SFX_CLANK);
        return 2;
    }
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
                shoot(bossX, bossY, sinTab[(a + 16) & 63] * 24 / 64, sinTab[a & 63] * 24 / 64, 0);
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
        shoot(bossX, bossY, dx * 27 / d, dy * 27 / d, 0);
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
            sfx(SFX_SLAM);
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

/* The Bronze Bell (web updBell): hangs above you, then slams down and rings out a circle of shots. */
void updBell(s16 dx, s16 dy, s16 d)
{
    u8 i;
    if (bossState == B_RISE)
    {
        bossZ += 1 + (bossT & 1);
        bossT++;
        if (bossZ > 40) bossZ = 40;
        if (!--bossSt) bossState = B_WALK;
        return;
    }
    if (bossState == B_DROP)
    {
        bossZ -= 4;
        if (bossZ <= 0)
        {
            bossZ = 0;
            bossState = B_STUN;
            bossSt = 110;
            sfx(SFX_SLAM);
            for (i = 0; i < 8; i++)
                shoot(bossX, bossY, sinTab[(i * 8 + 16) & 63] * 26 / 64, sinTab[(i * 8) & 63] * 26 / 64, 3);
        }
        return;
    }
    bossZ = 40;
    if (d > 4)
    {
        bossX += dx * 13 / d;
        bossY += dy * 13 / d;
    }
    bossT++;
    if (bossT % 140 == 139) bossState = B_DROP;
}

/* The Richmond Vampire (web updEnemy 'vampire'): fades in somewhere away from you, throws
   blood three ways, calls bats; at 60% it bursts into bats until you beat them, and at 30%
   it rings out blood every cycle. Gone (and safe from you) for the last 40 frames of each cycle. */
void vampShoot3(s16 dx, s16 dy, s16 d, s16 sp)
{
    s16 vx = dx * sp / d, vy = dy * sp / d;
    shoot(bossX, bossY - 6 * 16, vx, vy, 1);
    /* +-0.4 rad: cos 59/64, sin 25/64 */
    shoot(bossX, bossY - 6 * 16, (vx * 59 - vy * 25) / 64, (vx * 25 + vy * 59) / 64, 1);
    shoot(bossX, bossY - 6 * 16, (vx * 59 + vy * 25) / 64, (vy * 59 - vx * 25) / 64, 1);
}

void updVampire(s16 dx, s16 dy, s16 d)
{
    u8 i, ph;
    s16 x, y;
    if (!vampP2 && bossHp <= 8)
    {
        vampP2 = 1;
        vampSwarm = 1;
        for (i = 0; i < 3; i++)
            spawnEnemy(T_BAT, (bossX >> 4) + (i - 1) * 20, (bossY >> 4) - 10);
    }
    if (vampSwarm)
    {
        vampGone = 1;
        if (!enemyCount(T_BAT))
        {
            vampSwarm = 0;
            bossT = 0;
            vampGone = 0;
        }
        return;
    }
    if (!vampP3 && bossHp <= 4) vampP3 = 1;
    ph = bossT % 170;
    if (vampP3 && ph == 60)
        for (i = 0; i < 8; i++)
            shoot(bossX, bossY - 6 * 16, sinTab[(i * 8 + 16) & 63] * 24 / 64, sinTab[(i * 8) & 63] * 24 / 64, 1);
    vampGone = ph >= 130;
    if (ph == 0)
        for (i = 0; i < 30; i++)
        {
            x = (2 + rnd() % 12) * 16 + 8;
            y = (3 + rnd() % 6) * 16 + 8;
            if (!bigBlocked(x, y, 6) && ABS(x - px) + ABS(y - py) > 60)
            {
                bossX = x * 16;
                bossY = y * 16;
                break;
            }
        }
    if (ph == 40 || ph == 80 || (bossHp <= 7 && ph == 110))
        vampShoot3(dx, dy, d, bossHp <= 7 ? 30 : 24);
    if (ph == 100 && enemyCount(T_BAT) < 2)
        spawnEnemy(T_BAT, bossX >> 4, (bossY >> 4) - 10);
    bossT++;
}

/* The Titan Arum (web updArum): rooted in place, spits a fan of seeds and sprouts rats.
   Watered, it blooms (B_STUN) and can be hit until it closes again. */
void updArum(s16 dx, s16 dy, s16 d)
{
    s16 vx = dx * 22 / d, vy = dy * 22 / d;
    bossT++;
    if (bossT % 70 == 35)
    {
        shoot(bossX, bossY, vx, vy, 4);
        /* +-0.32 rad (cos 61/64, sin 20/64), and +-0.64 rad (51/64, 38/64) once it's hurt */
        shoot(bossX, bossY, (vx * 61 - vy * 20) / 64, (vx * 20 + vy * 61) / 64, 4);
        shoot(bossX, bossY, (vx * 61 + vy * 20) / 64, (vy * 61 - vx * 20) / 64, 4);
        if (bossHp <= 4)
        {
            shoot(bossX, bossY, (vx * 51 - vy * 38) / 64, (vx * 38 + vy * 51) / 64, 4);
            shoot(bossX, bossY, (vx * 51 + vy * 38) / 64, (vy * 51 - vx * 38) / 64, 4);
        }
    }
    if (bossT % 260 == 200 && enemyCount(T_RAT) < 2)
        spawnEnemy(T_RAT, (bossX >> 4) + ((bossT & 256) ? 30 : -30), (bossY >> 4) + 20);
}

/* The Raven (web updEnemy 'raven'): hangs above you, throws a feather, then dives. */
void updRaven(s16 dx, s16 dy, s16 d)
{
    s16 tx, ty, l;
    u8 ph;
    bossT++;
    ph = bossT % 120;
    if (ph < 70)
    {
        ty = py - 50;
        if (ty < 20) ty = 20;
        tx = px - (bossX >> 4);
        ty = ty - (bossY >> 4);
        l = ABS(tx) > ABS(ty) ? ABS(tx) + (ABS(ty) >> 1) : ABS(ty) + (ABS(tx) >> 1);
        if (l > 2)
        {
            bossX += tx * 11 / l;
            bossY += ty * 11 / l;
        }
        if (ph == 35) shoot(bossX, bossY, dx * 26 / d, dy * 26 / d, 2);
    }
    else
    {
        if (ph == 70)
        {
            bossVX = dx * 35 / d;
            bossVY = dy * 35 / d;
        }
        bossX += bossVX;
        bossY += bossVY;
    }
    if (bossX < 12 * 16) bossX = 12 * 16;
    if (bossY < 14 * 16) bossY = 14 * 16;
    if (bossX > (MAP_PX_W - 12) * 16) bossX = (MAP_PX_W - 12) * 16;
    if (bossY > (MAP_PX_H - 12) * 16) bossY = (MAP_PX_H - 12) * 16;
}

/* The Wraith (web updEnemy 'wraith'): drifts above you, throws orbs, calls ghosts. */
void updWraith(s16 dx, s16 dy, s16 d)
{
    s16 tx, ty;
    bossT++;
    if (bossVis)
    {
        bossVis--;
        bossX += sinTab[(bossT + (bossT >> 1) + (bossT >> 2)) & 63] * 5 / 64;
        return;
    }
    tx = px;
    if (tx < 40) tx = 40;
    if (tx > MAP_PX_W - 40) tx = MAP_PX_W - 40;
    ty = py - 56;
    if (ty < 30) ty = 30;
    if (ty > 90) ty = 90;
    bossX += (tx * 16 - bossX) / 50 + sinTab[((bossT >> 1) + 16) & 63] * 8 / 64;
    bossY += (ty * 16 - bossY) / 50;
    if (bossT % 110 == 60)
        shoot(bossX, bossY - 6 * 16, dx * 22 / d, (dy + 6) * 22 / d, 1);
    if (bossT % 330 == 200 && enemyCount(T_GHOST) < 2)
        spawnEnemy(T_GHOST, bossX >> 4, bossY >> 4);
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
            if (bossType == BOSS_BELL)
            {
                bossState = B_RISE;
                bossSt = 30;
            }
            else bossState = B_WALK;
            bossT = 0;
        }
        return; /* stunned bosses don't hurt */
    }
    if (bossType == BOSS_TURTLE) updTurtle(dx, dy);
    else if (bossType == BOSS_WRAITH) updWraith(dx, dy, d);
    else if (bossType == BOSS_RAVEN) updRaven(dx, dy, d);
    else if (bossType == BOSS_VAMPIRE)
    {
        updVampire(dx, dy, d);
        if (vampGone || vampSwarm) return;
    }
    else if (bossType == BOSS_ARUM)
    {
        updArum(dx, dy, d);
        return; /* rooted: it never touches you */
    }
    else if (bossType == BOSS_BELL)
    {
        updBell(dx, dy, d);
        if (bossZ > 12) return; /* up high: it can't touch you */
    }
    else updForeman(dx, dy, d);
    bx = bossX >> 4;
    by = bossY >> 4;
    if (ABS(bx - px) < (bossType >= BOSS_WRAITH ? 11 : 13) && ABS(by - py) < (bossType >= BOSS_WRAITH ? 11 : 13) && !dying)
        hurtHeroBy(bx, by, bossState == B_CHARGE || bossState == B_DROP || bossType == BOSS_VAMPIRE ? 2 : 1);
}

void drawBoss(void)
{
    u8 k, f, col;
    u16 id, t;
    s16 sx, sy, sh;
    for (k = 0; k < 9; k++)
    {
        id = (OAM_BOSS + k) * 4;
        /* the Wraith is see-through until its orb hits it: drawn every other frame */
        if (!bossOn || (bossFlash & 2) || (bossType == BOSS_WRAITH && !bossVis && (frameCount & 1)) ||
            (bossType == BOSS_VAMPIRE && (vampGone || vampSwarm || ((bossT % 170 < 15 || bossT % 170 > 115) && (frameCount & 1)))))
        {
            sprHide((id) >> 2);
            continue;
        }
        if (bossType == BOSS_WRAITH)
            f = (bossVis ? 0 : 2) + ((frameCount >> 3) & 1);
        else if (bossType == BOSS_RAVEN)
            f = (frameCount >> 3) & 1;
        else if (bossType == BOSS_BELL)
            f = bossState == B_STUN ? 1 : 0;
        else if (bossType == BOSS_ARUM)
            f = bossState == B_STUN ? 2 : (frameCount >> 5) & 1;
        else if (bossType == BOSS_VAMPIRE)
            f = 0;
        else if (bossType == BOSS_TURTLE)
            f = bossState == B_STUN ? 2 + ((frameCount >> 2) & 1) : (frameCount >> (bossState == B_CHARGE ? 1 : 3)) & 1;
        else
            f = bossState == B_STUN ? 3 : bossState == B_WIND ? 2 : (bossT >> 3) & 1;
        t = bossTiles[f * 9 + k];
        sh = bossState == B_WIND ? ((frameCount >> 1) & 1) * 2 - 1 : 0;
        /* the turtle's and the Raven's frames face right: mirror the 3x3 grid to face left */
        col = k % 3;
        if ((bossType == BOSS_TURTLE || bossType == BOSS_RAVEN || bossType == BOSS_VAMPIRE) && bossFlip) col = 2 - col;
        sx = (bossX >> 4) - camX - 24 + col * 16 + sh;
        sy = (bossY >> 4) - camY - 40 + (k / 3) * 16 - (bossType == BOSS_BELL ? bossZ : 0);
        if (!t || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            sprHide((id) >> 2);
            continue;
        }
        sprPut((id) >> 2, 1, sx, sy, 2, (bossType == BOSS_TURTLE || bossType == BOSS_RAVEN || bossType == BOSS_VAMPIRE) && bossFlip, t, PAL_BOSS);
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
            sfx(SFX_FUSE);
            bombX[i] = px + dirX[heroDir] * 12;
            bombY[i] = py + dirY[heroDir] * 12;
            powder--;
            return;
        }
}

void explode(s16 x, s16 y)
{
    sfx(SFX_BOOM);
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
                    sfx(SFX_SECRET);
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
            sprHide((id) >> 2);
            continue;
        }
        sprPut((id) >> 2, 1, sx, sy, 2, 0, OBJ_I_KEG, PAL_ITEM);
    }
    for (k = 0; k < 4; k++)
    {
        id = (OAM_BOOM + k) * 4;
        if (!boomT)
        {
            sprHide((id) >> 2);
            continue;
        }
        sx = boomX - camX - 16 + (k & 1) * 16;
        sy = boomY - camY - 16 + (k >> 1) * 16;
        sprPut((id) >> 2, 1, sx, sy, 3, 0, OBJ_I_BOOM + ((boomT < 8 ? 4 : 0) + k) * 2, PAL_ITEM);
    }
}

/* ---------- push blocks and switches (web tryPush, updBlocks) ---------- */

u8 *switchPtr(void)
{
    return blkPtr + 1 + blkPtr[0] * 2;
}

u8 onSwitch(u8 tx, u8 ty)
{
    u8 k;
    u8 *sw = switchPtr();
    for (k = 0; k < sw[0]; k++)
        if (sw[1 + k * 2] == tx && sw[2 + k * 2] == ty) return 1;
    return 0;
}

void blocksLoad(void)
{
    u8 k;
    u8 *sw = switchPtr();
    blkN = blkPtr[0];
    if (blkN > MAX_BLK) blkN = MAX_BLK;
    pushN = 0;
    for (k = 0; k < blkN; k++)
    {
        blkTX[k] = blkPtr[1 + k * 2];
        blkTY[k] = blkPtr[2 + k * 2];
        /* solved rooms keep their blocks on the switches (web load) */
        if ((roomFlags[curMap] & ROOM_SOLVED) && k < sw[0])
        {
            blkTX[k] = sw[1 + k * 2];
            blkTY[k] = sw[2 + k * 2];
        }
        blkX[k] = blkTX[k] * 16;
        blkY[k] = blkTY[k] * 16;
        blkMv[k] = 0;
    }
    if (blkN) dmaCopyCGram(&block_pal, 128 + PAL_BLOCK * 16, 16 * 2);
}

u8 blockAtCell(s16 tx, s16 ty)
{
    u8 k;
    for (k = 0; k < blkN; k++)
    {
        if (blkTX[k] == tx && blkTY[k] == ty) return k;
        if (blkMv[k] && blkTX[k] - blkDX[k] == tx && blkTY[k] - blkDY[k] == ty) return k;
    }
    return 255;
}

void notPushing(void)
{
    pushN = 0;
}

void tryPush(void)
{
    s16 fx = px + dirX[heroDir] * 8, fy = py + dirY[heroDir] * 8, nx, ny;
    u8 k, c;
    if (fx < 0 || fy < 0) return;
    k = blockAtCell(fx >> 4, fy >> 4);
    if (k == 255 || blkMv[k])
    {
        pushN = 0;
        return;
    }
    if (onSwitch(blkTX[k], blkTY[k])) return; /* locked onto the switch */
    if (pushDir != heroDir)
    {
        pushDir = heroDir;
        pushN = 0;
    }
    if (++pushN < 14) return;
    pushN = 0;
    nx = blkTX[k] + dirX[heroDir];
    ny = blkTY[k] + dirY[heroDir];
    if (nx < 0 || ny < 0 || nx >= mapW || ny >= mapH) return;
    c = colGrid[ny * COL_STRIDE + nx];
    if (c != CELL_OPEN || blockAtCell(nx, ny) != 255) return;
    blkTX[k] = nx;
    blkTY[k] = ny;
    blkDX[k] = dirX[heroDir];
    blkDY[k] = dirY[heroDir];
    blkMv[k] = 8;
}

void updBlocks(void)
{
    u8 k, j, all;
    u8 *sw;
    for (k = 0; k < blkN; k++)
        if (blkMv[k])
        {
            blkMv[k]--;
            blkX[k] += blkDX[k] * 2;
            blkY[k] += blkDY[k] * 2;
            if (blkMv[k] || roomSolve != 3 || (roomFlags[curMap] & ROOM_SOLVED)) continue;
            /* every switch covered: the room is solved */
            sw = switchPtr();
            all = sw[0] > 0;
            for (j = 0; j < sw[0]; j++)
                if (blockAtCell(sw[1 + j * 2], sw[2 + j * 2]) == 255) all = 0;
            if (all)
            {
                roomFlags[curMap] |= ROOM_SOLVED;
                sfx(SFX_SECRET);
                applyRoomState(0);
            }
        }
}

void drawBlocks(void)
{
    u8 k;
    for (k = 0; k < MAX_BLK; k++)
    {
        if (k >= blkN)
        {
            sprHide(OAM_BLOCKS + k);
            continue;
        }
        sprPut(OAM_BLOCKS + k, 1, blkX[k] - camX, blkY[k] - camY - 1, 2, 0, OBJ_I_BLOCK, PAL_BLOCK);
    }
}

void gotPiece(void)
{
    pieces++;
    if ((pieces & 3) == 0 && hpMax < 20)
    {
        hpMax += 2;
        hp = hpMax;
    }
    openTextAt((u8 *)&sys_txt + pieceTxt[pieces & 3]);
}

/* ---------- the Gold-Bug (web updBug) ---------- */

void throwBug(void)
{
    if (bugOn) return;
    bugOn = 1;
    bugRet = 0;
    bugT = 0;
    bugCarry = 255;
    bugHit = 0;
    bugX = px;
    bugY = py - 4;
    bugVX = dirX[heroDir] * 4;
    bugVY = dirY[heroDir] * 4;
}

/* water and pits don't stop it; walls do */
u8 bugBlocked(s16 x, s16 y)
{
    u8 c;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H) return 1;
    c = colGrid[(y >> 4) * COL_STRIDE + (x >> 4)];
    return c == CELL_SOLID || c == CELL_LOCK || c == CELL_COUNTER || c == CELL_BUSH || c == CELL_POST || c == CELL_BRAZIER;
}

void updBug(void)
{
    s16 dx, dy, d;
    u8 k;
    if (!bugOn) return;
    bugT++;
    if (!bugRet)
    {
        bugX += bugVX;
        bugY += bugVY;
        if (bugT >= 24 || bugBlocked(bugX, bugY)) bugRet = 1;
    }
    else
    {
        dx = px - bugX;
        dy = py - 4 - bugY;
        d = ABS(dx) > ABS(dy) ? ABS(dx) + (ABS(dy) >> 1) : ABS(dy) + (ABS(dx) >> 1);
        if (d < 6)
        {
            bugOn = 0;
            if (bugCarry != 255) collectPickup(bugCarry);
            return;
        }
        bugX += dx * 4 / d;
        bugY += dy * 4 / d;
    }
    if (bugCarry == 255)
    {
        k = pickupAt(bugX, bugY);
        if (k != 255)
        {
            bugCarry = k;
            bugRet = 1;
        }
    }
    else
        movePickup(bugCarry, bugX, bugY + 6);
    if (!bugHit && bugStunsEnemy(bugX, bugY))
    {
        bugHit = 1;
        bugRet = 1;
    }
}

void drawBug(void)
{
    if (!bugOn)
    {
        sprHide(OAM_BUG);
        return;
    }
    sprPut(OAM_BUG, 1, bugX - camX - 8, bugY - camY - 8, 3, 0, OBJ_I_BUG, PAL_ITEM);
}

/* ---------- items on B ---------- */

/* The Gas Lantern lights a cold brazier in front of you (web useItem 'lantern', lightAt). */
/* The Watering Can (web useItem 'can'): a sprout in front grows (and lays its vine bridges);
   near the Titan Arum, it blooms. */
void useCan(void)
{
    s16 fx = px + dirX[heroDir] * 14, fy = py + dirY[heroDir] * 14, bx, by;
    u8 k, n, tx = fx >> 4, ty = fy >> 4;
    u8 *g;
    if (fx >= 0 && fy >= 0 && fx < MAP_PX_W && fy < MAP_PX_H && colGrid[ty * COL_STRIDE + tx] == CELL_SPROUT)
    {
        g = dynPtr + 1;
        for (k = 0; k < dynPtr[0]; k++)
        {
            n = g[2];
            if (g[0] == 4 && g[4] == tx && g[5] == ty)
            {
                linkOpen[g[1] >> 3] |= 1 << (g[1] & 7);
                sfx(SFX_SECRET);
                applyRoomState(0);
                openTextAt((u8 *)&sys_txt + TXT_GROW);
                return;
            }
            g += 4 + n * 10;
        }
    }
    if (bossOn && bossType == BOSS_ARUM && bossState != B_STUN)
    {
        bx = (bossX >> 4) - fx;
        by = (bossY >> 4) - fy;
        if (ABS(bx) < 34 && ABS(by) < 34)
        {
            bossState = B_STUN;
            bossSt = 230;
            openTextAt((u8 *)&sys_txt + TXT_BLOOM);
        }
    }
}

void useLantern(void)
{
    s16 fx = px + dirX[heroDir] * 14, fy = py + dirY[heroDir] * 14;
    u8 k, all;
    u8 *r;
    if (chalAt(CH_LIGHT, fx, fy)) return; /* Light the Gas Lamps */
    for (k = 0; k < brazPtr[0]; k++)
    {
        r = brazPtr + 1 + k * 10;
        if (r[0] == (fx >> 4) && r[1] == (fy >> 4) && !(roomBraz[curMap] & (1 << k)))
        {
            roomBraz[curMap] |= 1 << k;
            queuePatch(r[0], r[1], r + 2, 0);
            all = (1 << brazPtr[0]) - 1;
            if (roomSolve == 2 && (roomBraz[curMap] & all) == all)
            {
                roomFlags[curMap] |= ROOM_SOLVED;
                sfx(SFX_SECRET);
                applyRoomState(0);
            }
            return;
        }
    }
}

void useItem(void)
{
    if (eqItem == ITEM_HOOK) fireHook();
    else if (eqItem == ITEM_POWDER) dropBomb();
    else if (eqItem == ITEM_LANTERN) useLantern();
    else if (eqItem == ITEM_BUG) throwBug();
    else if (eqItem == ITEM_CAN) useCan();
    else useGear();
}

u8 hasItem(u8 item)
{
    if (item == ITEM_HOOK) return hasHook;
    if (item == ITEM_POWDER) return hasPowder;
    if (item == ITEM_LANTERN) return hasLantern;
    if (item == ITEM_SHIELD) return hasShield;
    if (item == ITEM_BUG) return hasBug;
    if (item == ITEM_CAN) return hasCan;
    if (item == ITEM_BALLS) return hasBalls;
    if (item == ITEM_SHOES) return hasShoes;
    if (item == ITEM_BIKE) return hasBike;
    if (item == ITEM_BOTTLE) return hasBottle;
    return 0;
}

/* SELECT steps through the items you have for B (the web game picks them in its menu) */
void nextItem(void)
{
    u8 i, it = eqItem;
    for (i = 0; i < ITEM_BOTTLE; i++)
    {
        it = it >= ITEM_BOTTLE ? ITEM_HOOK : it + 1;
        if (it != ITEM_SHIELD && hasItem(it))
        {
            eqItem = it;
            return;
        }
    }
}

void gotItem(u8 item)
{
    sfx(SFX_JING);
    if (item == ITEM_HOOK) hasHook = 1;
    if (item == ITEM_POWDER)
    {
        hasPowder = 1;
        if (powder < 8) powder = 8; /* web give('powder') */
    }
    if (item == ITEM_LANTERN) hasLantern = 1;
    if (item == ITEM_BUG) hasBug = 1;
    if (item == ITEM_CAN) hasCan = 1;
    if (item >= ITEM_BALLS)
    {
        gotGear(item);
        if (item == ITEM_BOTTLE && eqItem) return; /* web: only if nothing's on B yet */
    }
    if (item == ITEM_SHIELD)
    {
        hasShield = 1; /* works on its own: not a B item */
        return;
    }
    eqItem = item;
}

void gotKeystone(u8 id)
{
    sfx(SFX_FANFARE);
    u8 n = 0, b;
    keystones |= 1 << id;
    for (b = 0; b < 4; b++)
        if (keystones & (1 << b)) n++;
    openTextAt((u8 *)&sys_txt + keystoneTxt[id * 4 + n - 1]);
}

void drawItemHud(void)
{
    u16 id = OAM_BICON * 4;
    if (eqItem == hudEq && powder == hudPowder && balls == hudBalls && bottleFull == hudBottle) return;
    hudEq = eqItem;
    hudPowder = powder;
    hudBalls = balls;
    hudBottle = bottleFull;
    if (!eqItem)
    {
        sprHide((id) >> 2);
        sprHide(((OAM_BICON + 1) * 4) >> 2);
        sprHide(((OAM_BICON + 2) * 4) >> 2);
        return;
    }
    sprPut((id) >> 2, 1, 120, 4, 3, 0, eqItem == ITEM_HOOK ? OBJ_I_HOOK : eqItem == ITEM_LANTERN ? OBJ_I_LANTERN : eqItem == ITEM_BUG ? OBJ_I_BUG : eqItem == ITEM_CAN ? OBJ_I_CAN : eqItem == ITEM_BALLS ? OBJ_I_BALL : eqItem == ITEM_SHOES ? OBJ_I_SHOES : eqItem == ITEM_BIKE ? OBJ_I_BIKE : eqItem == ITEM_BOTTLE ? OBJ_I_BOTTLE : OBJ_I_KEG, PAL_ITEM);
    if (eqItem == ITEM_POWDER) hudCount(powder);
    else if (eqItem == ITEM_BALLS) hudCount(balls);
    else
    {
        sprHide(((OAM_BICON + 1) * 4) >> 2);
        sprHide(((OAM_BICON + 2) * 4) >> 2);
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
            sprHide((id) >> 2);
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
                    sprHide((id) >> 2);
                    continue;
                }
                sx = px + dirX[hookDir] * d - camX - 4;
                sy = py - 4 + dirY[hookDir] * d - camY - 4;
            }
        }
        sprPut((id) >> 2, 0, sx, sy, 3, 0, k ? OBJ_CHAIN : OBJ_HOOK, PAL_HUD);
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
            sprHide((id) >> 2);
            continue;
        }
        sprPut((id) >> 2, 0, sx, sy, 3, 0, shotRefl[i] ? OBJ_REFL : shotKind[i] == 4 ? OBJ_SEED : shotKind[i] == 3 ? OBJ_RING : shotKind[i] == 2 ? OBJ_FEATHER : shotKind[i] ? OBJ_ORB : OBJ_SPARK + ((frameCount >> 1) & 1), PAL_HUD);
    }
}

void dungeonUpdate(void)
{
    updHook();
    updBug();
    updBlocks();
    updBombs();
    updBoss();
    updShots();
    checkCleared();
}

void dungeonDraw(void)
{
    drawBoss();
    drawBug();
    drawBlocks();
    drawBombs();
    drawItemHud();
    drawHook();
    drawShots();
}

/* Called after a map is loaded (forced blank). */
void dungeonLoad(void)
{
    u8 i;
    u8 *r;
    hookSt = 0;
    bugOn = 0;
    boomT = 0;
    for (i = 0; i < MAX_SHOT; i++) shotOn[i] = 0;
    for (i = 0; i < MAX_BOMB; i++) bombOn[i] = 0;
    applyRoomState(1);
    bossLoad();
    if (roomBoss == BOSS_FOREMAN)
    {
        /* boss art into the second OBJ name table after the items, palette 1 */
        dmaCopyVram(&boss1_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss1_chrend - &boss1_chr));
        dmaCopyCGram(&boss1_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_TURTLE)
    {
        dmaCopyVram(&boss2_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss2_chrend - &boss2_chr));
        dmaCopyCGram(&boss2_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_WRAITH)
    {
        dmaCopyVram(&boss3_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss3_chrend - &boss3_chr));
        dmaCopyCGram(&boss3_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_RAVEN)
    {
        dmaCopyVram(&boss4_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss4_chrend - &boss4_chr));
        dmaCopyCGram(&boss4_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_BELL)
    {
        dmaCopyVram(&boss5_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss5_chrend - &boss5_chr));
        dmaCopyCGram(&boss5_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_VAMPIRE)
    {
        dmaCopyVram(&boss7_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss7_chrend - &boss7_chr));
        dmaCopyCGram(&boss7_pal, 128 + 16, 16 * 2);
    }
    else if (roomBoss == BOSS_ARUM)
    {
        dmaCopyVram(&boss6_chr, VRAM_OBJ_CHR + 0x1000 + VRAM_BOSS_OFS, (&boss6_chrend - &boss6_chr));
        dmaCopyCGram(&boss6_pal, 128 + 16, 16 * 2);
    }
    blocksLoad();
    /* pickups lying in the room, unless already taken */
    for (i = 0; i < pickPtr[0]; i++)
    {
        r = pickPtr + 1 + i * 6;
        if (!(pickupGot[r[5] >> 3] & (1 << (r[5] & 7))))
            dropPickup(r[0] | (r[1] << 8), r[2] | (r[3] << 8), r[4], r[5]);
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
