/*---------------------------------------------------------------------------------
    Stage 3: the kayak paddle, hearts, the outdoor enemies, drops, and the fog taking you.
    Ported from the web game's play.js (updEnemy, hitEnemy, kill, hurt, the swing and spin).
    Enemy positions and speeds are in 1/16 px.
---------------------------------------------------------------------------------*/
#include "game.h"

u8 hp, hpMax, inv, atk, charging, charge, spin, kt;
s8 kx, ky;
u16 coins;
u8 dying;

u8 enCount;
u8 enType[MAX_EN], enHp[MAX_EN], enFlash[MAX_EN], enKt[MAX_EN], enStuck[MAX_EN], enFlip[MAX_EN], enAlive[MAX_EN], enStun[MAX_EN];
u8 enemiesExisted;
s16 enX[MAX_EN], enY[MAX_EN], enVX[MAX_EN], enVY[MAX_EN], enKX[MAX_EN], enKY[MAX_EN];
u16 enT[MAX_EN];

u8 pkKind[MAX_PK];
u8 enChal[MAX_EN]; /* spawned for a challenge */ /* 0 none, 1 coin, 2 heart, 3 heart container, 4 powder keg, 5-8 keystones, 9 small key */
u8 pkId[MAX_PK];   /* for pickups placed in a room: their id, else 255 */
s16 pkX[MAX_PK], pkY[MAX_PK];
u16 pkLife[MAX_PK];
u8 pkHop[MAX_PK]; /* frogs: frames to the next hop */

u16 seed;
extern u8 bugOn, bugCarry;
u8 hudHp, hudMax; /* what the HUD last showed */
u16 hudCoins;

void hitEnemy(u8 k, u8 dmg, s16 kdx, s16 kdy);
void collectPickup(u8 k);
u8 bossNear(s16 x, s16 y);
u16 rnd(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return seed;
}

/* web DX/DY order by our heroDir: 0 down, 1 up, 2 left, 3 right */
const s8 dirX[4] = {0, 0, -1, 1};
const s8 dirY[4] = {1, -1, 0, 0};
/* base paddle angle (16ths of a turn) for each heroDir: down, up, left, right */
const u8 baseAng[4] = {4, 12, 8, 0};

#define T_RAT 0
#define T_SQUIRREL 1
#define T_GOOSE 2
#define T_SCOOTER 3
#define T_CRAB 4
#define T_BAT 5
#define T_GHOST 6
#define FLIES(t) ((t) >= T_BAT)

/* RAM is not cleared at power-on, so every variable is set here. */
void combatInit(void)
{
    hpMax = 6;
    hp = 6;
    inv = 0;
    atk = 0;
    charging = 0;
    charge = 0;
    spin = 0;
    kt = 0;
    kx = 0;
    ky = 0;
    coins = 0;
    dying = 0;
    seed = 0xACE1;
    enCount = 0;
    hudHp = 255;
    hudMax = 255;
    hudCoins = 0xFFFF;
}

void combatLoad(u8 *ene)
{
    u8 k, n;
    n = ene[0];
    if (n > MAX_EN) n = MAX_EN;
    /* beaten rooms stay beaten (web: clr / boss flags); outdoors always refills */
    if (mapW != 32 && (roomFlags[curMap] & (ROOM_CLEARED | ROOM_BOSS))) n = 0;
    enCount = n;
    enemiesExisted = n > 0;
    for (k = 0; k < MAX_EN; k++)
    {
        enAlive[k] = 0;
        enChal[k] = 0;
        if (k < n)
        {
            enType[k] = ene[1 + k * 3];
            enX[k] = (ene[2 + k * 3] * 16 + 8) * 16;
            enY[k] = (ene[3 + k * 3] * 16 + 8) * 16;
            enHp[k] = enemyHp[enType[k]];
            enVX[k] = 0;
            enVY[k] = 0;
            enFlash[k] = 0;
            enKt[k] = 0;
            enStuck[k] = 0;
            enStun[k] = 0;
            enFlip[k] = 0;
            enT[k] = rnd() % 30;
            enAlive[k] = 1;
        }
    }
    for (k = 0; k < MAX_PK; k++)
        pkKind[k] = 0;
    atk = 0;
    spin = 0;
    charging = 0;
    kt = 0;
}

/* Walls and the map edge stop walking enemies. */
/* riverAt and enBlocked: src/collide.asm (stage 13e) */

/* Move enemy k by (dx, dy) sixteenths; returns 0 if a wall stopped it. */
u8 moveE(u8 k, s16 dx, s16 dy)
{
    u8 ok = 1;
    if (dx)
    {
        if (!enBlocked((enX[k] + dx) >> 4, enY[k] >> 4)) enX[k] += dx;
        else ok = 0;
    }
    if (dy)
    {
        if (!enBlocked(enX[k] >> 4, (enY[k] + dy) >> 4)) enY[k] += dy;
        else ok = 0;
    }
    return ok;
}

/* the 32 px paddle's art on show (stage 13c): the angle, and a pending copy for the VBlank */
u8 padA = 255, padPend;
u8 *padSrc;

void paddleVBlank(void)
{
    if (!padPend) return;
    dmaCopyVram(padSrc, VRAM_OBJ_CHR + OBJ_PADDLE2 * 16, 128);
    dmaCopyVram(padSrc + 128, VRAM_OBJ_CHR + (OBJ_PADDLE2 + 16) * 16, 128);
    dmaCopyVram(padSrc + 256, VRAM_OBJ_CHR + (OBJ_PADDLE2 + 32) * 16, 128);
    dmaCopyVram(padSrc + 384, VRAM_OBJ_CHR + (OBJ_PADDLE2 + 48) * 16, 128);
    padPend = 0;
}

s16 dist(s16 dx, s16 dy)
{
    s16 ax = ABS(dx), ay = ABS(dy);
    s16 d = ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);
    return d ? d : 1;
}

void pickRandomDir(u8 k, s16 speed)
{
    u8 a = rnd() & 3;
    enVX[k] = dirX[a] * speed;
    enVY[k] = dirY[a] * speed;
}

void dropPickup(s16 x, s16 y, u8 kind, u8 id)
{
    u8 i;
    for (i = 0; i < MAX_PK; i++)
        if (!pkKind[i])
        {
            pkKind[i] = kind;
            pkX[i] = x;
            pkY[i] = y;
            pkLife[i] = 600;
            pkId[i] = id;
            return;
        }
}

u8 pickupAt(s16 x, s16 y)
{
    u8 k;
    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k] && ABS(pkX[k] - x) < 9 && ABS(pkY[k] - y) < 9) return k;
    return 255;
}

void movePickup(u8 k, s16 x, s16 y)
{
    pkX[k] = x;
    pkY[k] = y;
}

/* The Gold-Bug stuns what it touches, and finishes off anything with 1 hit point left. */
u8 bugStunsEnemy(s16 x, s16 y)
{
    u8 k;
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k] && ABS((enX[k] >> 4) - x) < 10 && ABS((enY[k] >> 4) - y) < 10)
        {
            if (enHp[k] <= 1) hitEnemy(k, 1, 0, 0);
            else enStun[k] = 80;
            return 1;
        }
    return 0;
}

void dropKind(s16 x, s16 y, u8 kind)
{
    u8 i;
    for (i = 0; i < MAX_PK; i++)
        if (!pkKind[i])
        {
            pkId[i] = 255;
            pkKind[i] = kind;
            pkX[i] = x;
            pkY[i] = y;
            pkLife[i] = 600;
            return;
        }
}

/* the Wraith calls ghosts (web: enemies.push(mk('ghost', ...))) */
void spawnEnemy(u8 type, s16 x, s16 y)
{
    u8 k;
    for (k = 0; k < MAX_EN; k++)
        if (!enAlive[k])
        {
            enType[k] = type;
            enX[k] = x * 16;
            enY[k] = y * 16;
            enHp[k] = enemyHp[type];
            enVX[k] = 0;
            enVY[k] = 0;
            enFlash[k] = 0;
            enKt[k] = 0;
            enStuck[k] = 0;
            enStun[k] = 0;
            enFlip[k] = 0;
            enT[k] = 0;
            enChal[k] = 0;
            enAlive[k] = 1;
            return;
        }
}

u8 enemyCount(u8 type)
{
    u8 k, n = 0;
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k] && enType[k] == type) n++;
    return n;
}

u8 enemiesAlive(void)
{
    u8 k;
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k]) return 1;
    return 0;
}

/* The hook stuns what it hits (web updHook). */
u8 hookHitsEnemy(s16 x, s16 y, s16 kx, s16 ky)
{
    u8 k;
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k] && ABS((enX[k] >> 4) - x) < 9 && ABS((enY[k] >> 4) - 6 - y) < 12)
        {
            hitEnemy(k, 1, kx, ky);
            if (enAlive[k]) enStun[k] = 60;
            return 1;
        }
    return 0;
}

/* ...and brings back what it touches */
void hookGrabsPickup(s16 x, s16 y)
{
    u8 k;
    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k] && ABS(pkX[k] - x) < 9 && ABS(pkY[k] - y) < 9)
            collectPickup(k);
}

/* what a beaten enemy leaves behind (web kill()) */
void dropAt(s16 x, s16 y)
{
    u8 r = rnd() % 100;
    if (r < 30) dropKind(x, y, 1);
    else if (r < 44) dropKind(x, y, 2);
    else if (r < 52 && hasBalls) dropKind(x, y, 13);
    else if (r < 57 && hasPowder) dropKind(x, y, 4);
}

/* web explode(): 2 damage to everything within 30px, knocked away */
void bombHitsEnemies(s16 x, s16 y)
{
    u8 k;
    s16 dx, dy;
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k])
        {
            dx = (enX[k] >> 4) - x;
            dy = (enY[k] >> 4) - y;
            if (ABS(dx) < 30 && ABS(dy) < 30)
                hitEnemy(k, 2, dx > 0 ? 48 : -48, dy > 0 ? 48 : -48);
        }
}

static u16 partText(u8 gid)
{
    if (gid == PART_GID0) return parts == 1 ? TXT_PART0_1 : parts == 2 ? TXT_PART0_2 : TXT_PART0_3;
    if (gid == PART_GID1) return parts == 1 ? TXT_PART1_1 : parts == 2 ? TXT_PART1_2 : TXT_PART1_3;
    return parts == 1 ? TXT_PART2_1 : parts == 2 ? TXT_PART2_2 : TXT_PART2_3;
}

void collectPickup(u8 k)
{
    u8 kind = pkKind[k];
    pkKind[k] = 0;
    if (kind == PK_FROG)
    {
        gotFrog();
        return;
    }
    if (kind == PK_TOKEN || kind == PK_FLAG)
    {
        chalProgress(); /* a challenge token or the finish flag */
        return;
    }
    sfx(kind == 1 || kind == 4 || kind == 13 ? SFX_COIN : kind == 2 ? SFX_HEART : SFX_JING);
    if (kind == 1) { if (coins < 999) coins++; }
    else if (kind == 2) { hp += 2; if (hp > hpMax) hp = hpMax; }
    else if (kind == 3)
    {
        if (hpMax < 20) hpMax += 2;
        hp = hpMax;
        roomFlags[curMap] |= ROOM_CONTAINER;
        openTextAt((u8 *)&sys_txt + TXT_CONTAINER);
    }
    else if (kind == 4) { if (powder < capPowder) powder++; }
    else if (kind == 13) { balls = balls + 3 > capBalls ? capBalls : balls + 3; }
    else if (kind == 14)
    {
        /* a bateau part for the river historian (web collect 'part') */
        parts++;
        openTextAt((u8 *)&sys_txt + partText(pkId[k]));
    }
    else if (kind == 12)
    {
        hasBib = 1;
        openTextAt((u8 *)&sys_txt + TXT_BIB);
    }
    else if (kind == 9)
    {
        keys[roomArea]++;
        openTextAt((u8 *)&sys_txt + TXT_SKEY);
    }
    else if (kind == 10)
    {
        hasCarkey = 1;
        openTextAt((u8 *)&sys_txt + TXT_CARKEY_GOT);
    }
    else if (kind == 11) gotPiece();
    else gotKeystone(kind - 5);
    if (pkId[k] != 255) pickupGot[pkId[k] >> 3] |= 1 << (pkId[k] & 7);
}

void hitEnemy(u8 k, u8 dmg, s16 kdx, s16 kdy)
{
    if (!enAlive[k] || enFlash[k]) return;
    enFlash[k] = 16;
    enKX[k] = kdx;
    enKY[k] = kdy;
    enKt[k] = 8;
    if (enHp[k] <= dmg)
    {
        enAlive[k] = 0;
        sfx(SFX_KILL);
        dropAt(enX[k] >> 4, enY[k] >> 4);
        if (enChal[k]) chalKill(k);
    }
    else
    {
        enHp[k] -= dmg;
        sfx(SFX_HIT);
    }
}

void hurtHero(s16 sx, s16 sy)
{
    hurtHeroBy(sx, sy, 1);
}

void hurtHeroBy(s16 sx, s16 sy, u8 dmg)
{
    s16 dx, dy, d;
    if (inv || hp == 0 || dying || hookSt == 3 || dash) return;
    riding = 0;
    sfx(SFX_HURT);
    hp = hp > dmg ? hp - dmg : 0;
    inv = 50;
    dx = px - sx;
    dy = py - sy;
    d = dist(dx, dy);
    kx = (dx * 5) / (d * 2);
    ky = (dy * 5) / (d * 2);
    if (!kx && !ky) ky = 2;
    kt = 8;
    atk = 0;
    charging = 0;
    spin = 0;
    if (hp == 0)
    {
        dying = 1;
        openTextAt((u8 *)&sys_txt + (dentMap != 255 ? TXT_DEADIN : TXT_DEAD));
    }
}

void updEnemy(u8 k)
{
    s16 dx, dy, d, ex, ey;
    u16 ph, t;
    u8 ty = enType[k];

    if (enFlash[k]) enFlash[k]--;
    if (enStun[k])
    {
        enStun[k]--;
        return;
    }
    if (enKt[k])
    {
        enKt[k]--;
        if (FLIES(ty)) { enX[k] += enKX[k]; enY[k] += enKY[k]; }
        else moveE(k, enKX[k], enKY[k]);
        return;
    }
    t = ++enT[k];
    ex = enX[k] >> 4;
    ey = enY[k] >> 4;
    dx = px - ex;
    dy = py - ey;
    d = dist(dx, dy);

    switch (ty)
    {
    case T_RAT:
        if (t % 45 == 1 || enStuck[k]) pickRandomDir(k, 13);
        enStuck[k] = !moveE(k, enVX[k], enVY[k]);
        enFlip[k] = enVX[k] < 0;
        break;
    case T_SQUIRREL:
        ph = t % 70;
        if (ph == 40) { enVX[k] = dx * 27 / d; enVY[k] = dy * 27 / d; }
        if (ph >= 40 && ph < 62) moveE(k, enVX[k], enVY[k]);
        enFlip[k] = dx < 0;
        break;
    case T_GOOSE:
        if (d < 70)
        {
            enVX[k] = dx * 12 / d;
            enVY[k] = dy * 12 / d;
            moveE(k, enVX[k], enVY[k]);
        }
        else
        {
            if (t % 60 == 1 || enStuck[k]) pickRandomDir(k, 6);
            enStuck[k] = !moveE(k, enVX[k], enVY[k]);
        }
        enFlip[k] = enVX[k] < 0;
        break;
    case T_SCOOTER:
        if ((!enVX[k] && !enVY[k]) || enStuck[k] || rnd() % 100 == 0) pickRandomDir(k, 22);
        enStuck[k] = !moveE(k, enVX[k], enVY[k]);
        enFlip[k] = enVX[k] < 0;
        break;
    case T_CRAB:
        if (t % 60 == 1 || enStuck[k])
        {
            enVX[k] = (rnd() & 1) ? -19 : 19;
            enVY[k] = (rnd() % 10 < 3) ? (dy > 0 ? 6 : -6) : 0;
        }
        enStuck[k] = !moveE(k, enVX[k], enVY[k]);
        enFlip[k] = enVX[k] < 0;
        break;
    case T_BAT:
        /* toward the hero at 0.6 px, wobbling on cos(t/9) and sin(t/7) at 0.8 px */
        enX[k] += dx * 10 / d + sinTab[(t + (t >> 3) + 16) & 63] / 5;
        enY[k] += dy * 10 / d + sinTab[(t + (t >> 1)) & 63] / 5;
        enFlip[k] = dx < 0;
        break;
    case T_GHOST:
        enX[k] += dx * 6 / d;
        enY[k] += dy * 6 / d;
        enFlip[k] = dx < 0;
        break;
    }
    if (FLIES(ty))
    {
        if (enX[k] < 12 * 16) enX[k] = 12 * 16;
        if (enY[k] < 14 * 16) enY[k] = 14 * 16;
        if (enX[k] > (MAP_PX_W - 12) * 16) enX[k] = (MAP_PX_W - 12) * 16;
        if (enY[k] > (MAP_PX_H - 12) * 16) enY[k] = (MAP_PX_H - 12) * 16;
    }
}

/* Is the hero mid-swing, spinning, knocked back or on the hook (so they can't walk)? */
u8 heroBusy(void)
{
    return atk || spin || kt || hookSt || dash;
}

extern s16 bossX, bossY;
u8 bossNear(s16 x, s16 y)
{
    return ABS((bossX >> 4) - x) < 14 && ABS((bossY >> 4) - 8 - y) < 18;
}

void combatUpdate(u16 pad, u16 pressed)
{
    u8 k;
    s16 hx, hy, dx, dy, d, ex, ey;

    if (inv) inv--;

    /* knockback after being hit */
    if (kt)
    {
        kt--;
        /* knockback never carries you over the edge into the next neighbourhood */
        if (kx && !blocked(px + kx, py) && px + kx > HALF && px + kx < MAP_PX_W - HALF - 1) px += kx;
        if (ky && !blocked(px, py + ky) && py + ky > HALF && py + ky < MAP_PX_H - HALF - 1) py += ky;
        lastCell = (py >> 4) * COL_STRIDE + (px >> 4); /* knocked onto stairs isn't taking them */
    }

    /* B uses the item you've picked; SELECT picks the next one */
    if ((pressed & KEY_B) && !atk && !spin && !charging && !hookSt)
        useItem();
    if (pressed & KEY_SELECT)
        nextItem();

    /* A swings (main.c calls interact() first and only swings if nothing was there) */
    if ((pressed & KEY_A) && !atk && !spin && !hookSt)
    {
        if (riding) riding = 0; /* A hops off the bike first */
        else
        {
            atk = 14;
            sfx(SFX_SWING);
            charging = 0;
        }
    }

    if (spin)
    {
        spin--;
        for (k = 0; k < MAX_EN; k++)
            if (enAlive[k])
            {
                dx = (enX[k] >> 4) - px;
                dy = (enY[k] >> 4) - py;
                d = dist(dx, dy);
                if (d < 24) hitEnemy(k, goldPaddle ? 4 : 2, dx * 48 / d, dy * 48 / d);
            }
        if (bossOn && bossNear(px, py)) bossHit(2, 0);
        /* web: every 4th spin frame, cut at 8 points 16px out */
        if ((spin & 3) == 0)
        {
            cutAt(px + 16, py - 2);
            cutAt(px + 11, py + 9);
            cutAt(px, py + 14);
            cutAt(px - 11, py + 9);
            cutAt(px - 16, py - 2);
            cutAt(px - 11, py - 13);
            cutAt(px, py - 18);
            cutAt(px + 11, py - 13);
        }
    }
    else if (atk)
    {
        atk--;
        if (atk == 0 && (pad & KEY_A))
        {
            charging = 1;
            charge = 0;
        }
    }
    else if (charging)
    {
        if (pad & KEY_A)
        {
            if (charge < 60) charge++;
        }
        else
        {
            if (charge >= 40)
            {
                spin = 22;
                sfx(SFX_SWING);
            }
            charging = 0;
            charge = 0;
        }
    }

    if (atk > 5)
    {
        hx = px + dirX[heroDir] * 12;
        hy = py - 2 + dirY[heroDir] * 12;
        for (k = 0; k < MAX_EN; k++)
            if (enAlive[k])
            {
                ex = enX[k] >> 4;
                ey = enY[k] >> 4;
                if (ABS(ex - hx) < 11 && ABS(ey - hy) < 11)
                    hitEnemy(k, goldPaddle ? 2 : 1, dirX[heroDir] * 40, dirY[heroDir] * 40); /* the gold paddle hits twice as hard */
            }
        if (atk == 10) cutAt(hx, hy);
        if (bossOn && bossNear(hx, hy) && bossHit(goldPaddle ? 2 : 1, 0) == 2)
        {
            /* clank: the paddle bounces off the armour */
            kx = -dirX[heroDir] * 2;
            ky = -dirY[heroDir] * 2;
            kt = 6;
            atk = 0;
        }
    }

    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k])
        {
            updEnemy(k);
            if (!dying && !enStun[k] && ABS((enX[k] >> 4) - px) < 9 && ABS((enY[k] >> 4) - py) < 9)
                hurtHero(enX[k] >> 4, enY[k] >> 4);
        }

    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k])
        {
            if ((pkKind[k] < 3 || pkKind[k] == PK_FROG) && !--pkLife[k])
            {
                pkKind[k] = 0;
                continue;
            }
            if (ABS(pkX[k] - px) < 10 && ABS(pkY[k] - py) < 10 && !(bugOn && bugCarry == k))
                collectPickup(k);
        }
}

void combatDraw(void)
{
    u8 k, a, show;
    u16 id, tile;
    s16 sx, sy;

    /* the paddle */
    id = OAM_PADDLE * 4;
    show = 1;
    if (spin) a = (baseAng[heroDir] + spinStep[spin]) & 15;
    else if (atk) a = (baseAng[heroDir] + swingStep[atk]) & 15;
    else if (charging) a = baseAng[heroDir];
    else show = 0;
    /* a charged paddle blinks to say "let go to spin" */
    if (charging && charge >= 40 && (frameCount & 4)) show = 0;
    if (show)
    {
        /* stage 13c: 32 px, four sprites (OAM 15, 11, 12, 63); this angle's art streams in at the next VBlank */
        if (a != padA)
        {
            padA = a;
            padSrc = (u8 *)&paddle2_chr + ((u16)a << 9);
            padPend = 1;
        }
        sx = SX(px) + paddleOx[a] - 8;
        sy = SY(py) + paddleOy[a] - 8;
        sprPut(OAM_PADDLE, 1, sx, sy, 3, 0, OBJ_PADDLE2, PAL_HUD);
        sprPut(OAM_PADDLE2, 1, sx + 16, sy, 3, 0, OBJ_PADDLE2 + 2, PAL_HUD);
        sprPut(OAM_PADDLE3, 1, sx, sy + 16, 3, 0, OBJ_PADDLE2 + 32, PAL_HUD);
        sprPut(OAM_PADDLE4, 1, sx + 16, sy + 16, 3, 0, OBJ_PADDLE2 + 34, PAL_HUD);
    }
    else
    {
        sprHide(OAM_PADDLE);
        sprHide(OAM_PADDLE2);
        sprHide(OAM_PADDLE3);
        sprHide(OAM_PADDLE4);
    }

    for (k = 0; k < MAX_EN; k++)
    {
        id = (OAM_ENEMIES + k) * 4;
        sx = SX(enX[k] >> 4) - 8;
        sy = SY(enY[k] >> 4) - 9;
        if (!enAlive[k] || (enFlash[k] & 2) || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            sprHide((id) >> 2);
            continue;
        }
        tile = enemyTile[enType[k]];
        if ((frameCount + k * 5) & 8) tile += 2; /* the second frame (stage 12): everything moves, a little out of step */
        sprPut((id) >> 2, 1, sx, sy, 2, enFlip[k], tile, enemyPal[enType[k]]);
    }

    for (k = 0; k < MAX_PK; k++)
    {
        id = (OAM_PICKUPS + k) * 4;
        if (!pkKind[k])
        {
            sprHide((id) >> 2);
            continue;
        }
        sx = SX(pkX[k]) - 4;
        sy = SY(pkY[k]) - 5;
        if (!pkKind[k] || (pkKind[k] < 3 && pkLife[k] < 120 && (frameCount & 4)) || sx < -8 || sx > SCREEN_W || sy < -8 || sy > SCREEN_H)
        {
            sprHide((id) >> 2);
            continue;
        }
        if (pkKind[k] >= 3)
        {
            tile = pkKind[k] == 3 ? OBJ_I_CONTAINER : pkKind[k] == 4 ? OBJ_I_KEG : pkKind[k] == 9 ? OBJ_I_KEY : pkKind[k] == 10 ? OBJ_I_CARKEY :
                   pkKind[k] == 11 ? OBJ_I_PIECE : pkKind[k] == 12 ? OBJ_I_BIB : pkKind[k] == 13 ? OBJ_I_BALL : pkKind[k] == 14 ? OBJ_I_OAR :
                   pkKind[k] == PK_TOKEN ? (chalTok[chalOn - 1] == 255 ? OBJ_I_BALL : townTile[chalTok[chalOn - 1]]) :
                   pkKind[k] == PK_FLAG ? townTile[TOWN_FLAG + ((frameCount >> 3) & 1)] : pkKind[k] == PK_FROG ? OBJ_I_FROG : OBJ_I_KEYSTONE + (pkKind[k] - 5) * 2;
            sprPut((id) >> 2, 1, (sx - 4), (sy - 4 - (pkKind[k] == PK_FROG && pkHop[k] > 0 && pkHop[k] < 6 ? 3 : 0)), 2, 0, tile,
                   pkKind[k] == PK_TOKEN ? chalPal[chalOn - 1] : pkKind[k] == PK_FROG ? PAL_FROG : PAL_ITEM);
            continue;
        }
        tile = pkKind[k] == 1 ? HUD_COIN + ((frameCount >> 3) & 1) : HUD_HEART;
        sprPut((id) >> 2, 0, sx, sy, 2, 0, tile, PAL_HUD);
    }
}

void hudDraw(void)
{
    u8 i, d0, d1, d2;
    u16 id, tile;
    /* the HUD only changes when health or coins do */
    if (hp == hudHp && hpMax == hudMax && coins == hudCoins) return;
    hudHp = hp;
    hudMax = hpMax;
    hudCoins = coins;
    for (i = 0; i < 10; i++)
    {
        id = (OAM_HEARTS + i) * 4;
        if (i * 2 >= hpMax)
        {
            sprHide((id) >> 2);
            continue;
        }
        tile = hp >= i * 2 + 2 ? HUD_HEART : hp == i * 2 + 1 ? HUD_HALF : HUD_EMPTY;
        sprPut((id) >> 2, 0, 8 + i * 9, 8, 3, 0, tile, PAL_HUD);
    }
    sprPut((OAM_COIN * 4) >> 2, 0, 216, 8, 3, 0, HUD_COIN, PAL_HUD);
    d2 = coins % 10;
    d1 = (coins / 10) % 10;
    d0 = coins / 100;
    for (i = 0; i < 3; i++)
    {
        id = (OAM_DIGITS + i) * 4;
        if ((i == 0 && !d0) || (i == 1 && !d0 && !d1))
        {
            sprHide((id) >> 2);
            continue;
        }
        sprPut((id) >> 2, 0, 226 + i * 8, 8, 3, 0, HUD_DIGIT + (i == 0 ? d0 : i == 1 ? d1 : d2), PAL_HUD);
    }
}
