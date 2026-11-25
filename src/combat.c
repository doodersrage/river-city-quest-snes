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
u8 enType[MAX_EN], enHp[MAX_EN], enFlash[MAX_EN], enKt[MAX_EN], enStuck[MAX_EN], enFlip[MAX_EN], enAlive[MAX_EN];
s16 enX[MAX_EN], enY[MAX_EN], enVX[MAX_EN], enVY[MAX_EN], enKX[MAX_EN], enKY[MAX_EN];
u16 enT[MAX_EN];

u8 pkKind[MAX_PK]; /* 0 none, 1 coin, 2 heart */
s16 pkX[MAX_PK], pkY[MAX_PK];
u16 pkLife[MAX_PK];

u16 seed;
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
}

void combatLoad(u8 *ene)
{
    u8 k, n;
    n = ene[0];
    if (n > MAX_EN) n = MAX_EN;
    enCount = n;
    for (k = 0; k < MAX_EN; k++)
    {
        enAlive[k] = 0;
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
u8 enBlocked(s16 x, s16 y)
{
    if (x - HALF < 0 || y - HALF < 0 || x + HALF >= MAP_PX_W || y + HALF >= MAP_PX_H) return 1;
    if (solidAt(x - HALF, y - HALF)) return 1;
    if (solidAt(x + HALF, y - HALF)) return 1;
    if (solidAt(x - HALF, y + HALF)) return 1;
    if (solidAt(x + HALF, y + HALF)) return 1;
    return 0;
}

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

void dropAt(s16 x, s16 y)
{
    u8 i, r = rnd() % 100, kind;
    if (r < 30) kind = 1;
    else if (r < 44) kind = 2;
    else return;
    for (i = 0; i < MAX_PK; i++)
        if (!pkKind[i])
        {
            pkKind[i] = kind;
            pkX[i] = x;
            pkY[i] = y;
            pkLife[i] = 600;
            return;
        }
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
        dropAt(enX[k] >> 4, enY[k] >> 4);
    }
    else
        enHp[k] -= dmg;
}

void hurtHero(s16 sx, s16 sy)
{
    s16 dx, dy, d;
    if (inv || hp == 0 || dying) return;
    hp = hp > 1 ? hp - 1 : 0;
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
        openTextAt((u8 *)&sys_txt + TXT_DEAD);
    }
}

void updEnemy(u8 k)
{
    s16 dx, dy, d, ex, ey;
    u16 ph, t;
    u8 ty = enType[k];

    if (enFlash[k]) enFlash[k]--;
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

/* Is the hero mid-swing or spinning (so they can't walk)? */
u8 heroBusy(void)
{
    return atk || spin || kt;
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
        if (kx && !blocked(px + kx, py)) px += kx;
        if (ky && !blocked(px, py + ky)) py += ky;
    }

    /* A swings (main.c calls interact() first and only swings if nothing was there) */
    if ((pressed & KEY_A) && !atk && !spin)
    {
        atk = 14;
        charging = 0;
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
                if (d < 24) hitEnemy(k, 2, dx * 48 / d, dy * 48 / d);
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
            if (charge >= 40) spin = 22;
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
                    hitEnemy(k, 1, dirX[heroDir] * 40, dirY[heroDir] * 40);
            }
    }

    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k])
        {
            updEnemy(k);
            if (!dying && ABS((enX[k] >> 4) - px) < 9 && ABS((enY[k] >> 4) - py) < 9)
                hurtHero(enX[k] >> 4, enY[k] >> 4);
        }

    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k])
        {
            if (!--pkLife[k])
            {
                pkKind[k] = 0;
                continue;
            }
            if (ABS(pkX[k] - px) < 10 && ABS(pkY[k] - py) < 10)
            {
                if (pkKind[k] == 1) { if (coins < 999) coins++; }
                else { hp += 2; if (hp > hpMax) hp = hpMax; }
                pkKind[k] = 0;
            }
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
        sx = px - camX + paddleOx[a];
        sy = py - camY + paddleOy[a];
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 3, 0, 0, paddleTile[a], PAL_HUD);
    }
    else
        oamSetEx(id, OBJ_LARGE, OBJ_HIDE);

    for (k = 0; k < MAX_EN; k++)
    {
        id = (OAM_ENEMIES + k) * 4;
        sx = (enX[k] >> 4) - camX - 8;
        sy = (enY[k] >> 4) - camY - 9;
        if (!enAlive[k] || (enFlash[k] & 2) || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        tile = enemyTile[enType[k]];
        if (enType[k] == T_BAT && (frameCount & 8)) tile = BAT_TILE2;
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, enFlip[k], 0, tile, enemyPal[enType[k]]);
    }

    for (k = 0; k < MAX_PK; k++)
    {
        id = (OAM_PICKUPS + k) * 4;
        sx = pkX[k] - camX - 4;
        sy = pkY[k] - camY - 5;
        if (!pkKind[k] || (pkLife[k] < 120 && (frameCount & 4)) || sx < -8 || sx > SCREEN_W || sy < -8 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
            continue;
        }
        tile = pkKind[k] == 1 ? HUD_COIN + ((frameCount >> 3) & 1) : HUD_HEART;
        oamSetEx(id, OBJ_SMALL, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, 0, 0, tile, PAL_HUD);
    }
}

void hudDraw(void)
{
    u8 i, d0, d1, d2;
    u16 id, tile;
    for (i = 0; i < 10; i++)
    {
        id = (OAM_HEARTS + i) * 4;
        if (i * 2 >= hpMax)
        {
            oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
            continue;
        }
        tile = hp >= i * 2 + 2 ? HUD_HEART : hp == i * 2 + 1 ? HUD_HALF : HUD_EMPTY;
        oamSetEx(id, OBJ_SMALL, OBJ_SHOW);
        oamSet(id, 8 + i * 9, 8, 3, 0, 0, tile, PAL_HUD);
    }
    oamSetEx(OAM_COIN * 4, OBJ_SMALL, OBJ_SHOW);
    oamSet(OAM_COIN * 4, 216, 8, 3, 0, 0, HUD_COIN, PAL_HUD);
    d2 = coins % 10;
    d1 = (coins / 10) % 10;
    d0 = coins / 100;
    for (i = 0; i < 3; i++)
    {
        id = (OAM_DIGITS + i) * 4;
        if ((i == 0 && !d0) || (i == 1 && !d0 && !d1))
        {
            oamSetEx(id, OBJ_SMALL, OBJ_HIDE);
            continue;
        }
        oamSetEx(id, OBJ_SMALL, OBJ_SHOW);
        oamSet(id, 226 + i * 8, 8, 3, 0, 0, HUD_DIGIT + (i == 0 ? d0 : i == 1 ? d1 : d2), PAL_HUD);
    }
}
