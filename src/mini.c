/* River City Quest (SNES): minigames (web minigames.js, rva.js). Batting practice at The Diamond, Rams free
   throws in the Siegel Center and tennis at the Ashe Center. Each takes over the screen: its court or field on
   BG1 (the web game's 256x200, 12 px down), a score line and a message on BG3, and 8x8 sprites for what moves. */
#include <snes.h>
#include "game.h"

#define MY 12                 /* the web screen starts 12 px down */
#define OAM_M 16              /* the minigame's sprites: 16 on */
#define PLATE 166             /* web PLATE */

extern u16 padSeen;
extern void rcqLoadMini(u8 k);
void flushText(void);

u8 miniOn;
u16 miniBest[8];              /* best score per game (higher is better) */

u8 mgKind, mgDone, mgQuit;
u16 mgT;
/* shared game state */
u8 mgN, mgHits, mgHr, mgMade, mgRet, mgMiss, mgWait, mgSwing, mgRes;
u8 mgBallOn, mgBallUp, mgAge, mgArr;
s16 mgBx, mgBy, mgBvx, mgBvy;  /* ball, 1/64 px */
s16 mgHx;                /* the tennis player, 1/16 px */
u16 mgPh;                /* free-throw needle phase */
u8 mgSx0, mgSx1, mgBt;       /* free-throw flight: start/end x, frames */
static char msg[33], top[33];
static u8 msgDirty, topDirty;
static u16 row[32], rowTop[32], rowMsg[32];
static u8 topReady, msgReady, albumRow; /* the top line's row: 0, or 26 in the album */

/* ---------- text on BG3 rows 0 (score) and 27 (messages), outside the picture ---------- */
/* lay a line out centred in buf, as BG3 map words */
static void bg3Fill(u16 *buf, const char *s, u16 font)
{
    u8 c, k = 0;
    while (s[k] && k < 32) k++;
    for (c = 0; c < 32; c++) buf[c] = 0;
    for (c = 0; c < k; c++) buf[(32 - k) / 2 + c] = (font + s[c] - 32) | 0x2000;
}

static void bg3Row(u8 r, const char *s, u16 font)
{
    u8 c, k = 0;
    while (s[k] && k < 32) k++;
    for (c = 0; c < 32; c++) row[c] = 0;
    for (c = 0; c < k && c < 32; c++) row[(32 - k) / 2 + c] = (font + s[c] - 32) | 0x2000;
    dmaCopyVram((u8 *)row, VRAM_BG3_MAP + r * 32, 64);
}

static char *putStr(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *putNum(char *d, u16 v)
{
    char b[6];
    u8 k = 0;
    do
    {
        b[k++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (k) *d++ = b[--k];
    *d = 0;
    return d;
}

static void setMsg(const char *s)
{
    putStr(msg, s);
    msgDirty = 1;
}

static void put8(u8 i, s16 x, s16 y, u16 tile, u8 pal)
{
    if (x < -8 || x > 256 || y < -8 || y > 224) sprHide(OAM_M + i);
    else sprPut(OAM_M + i, 0, x, y, 3, 0, tile, pal);
}

/* a bat or racket: dots of 2x2 along angle a (64ths of a turn) from (x, y), r from r0 to r1, tip from rt */
static void stick(u8 first, s16 x, s16 y, u8 a, u8 r0, u8 r1, u8 rt, u16 tile, u16 tipTile)
{
    u8 r, i = first;
    s16 c = sinTab[(a + 16) & 63], s = sinTab[a & 63];
    for (r = r0; r <= r1; r += 2, i++)
        put8(i, x + ((c * r) >> 6), y + ((s * r) >> 6), r > rt ? tipTile : tile, 1);
}

/* ---------- batting practice (web batUpdate) ---------- */
static const u8 batA[13] = {40, 17, 13, 10, 6, 3, 63, 59, 56, 52, 49, 45, 42}; /* by swing frames left */

static void batTop(void)
{
    char *d = putStr(top, "Pitch ");
    d = putNum(d, mgN < 10 ? mgN + 1 : 10);
    d = putStr(d, "/10   Hits ");
    d = putNum(d, mgHits);
    d = putStr(d, "   HR ");
    putNum(d, mgHr);
    topDirty = 1;
}

static void batUpdate(u16 pressed)
{
    s16 d;
    u8 ad;
    if (mgSwing) mgSwing--;
    if (!mgBallOn)
    {
        if (mgN >= 10)
        {
            mgDone = 1;
            return;
        }
        if (!--mgWait)
        {
            /* the pitch: 1.6-3.2 px a frame, a little curve */
            mgBvy = 102 + (rnd() & 63);
            mgBvx = (s16)(rnd() & 31) - 16;
            mgBx = 128 << 6;
            mgBy = 58 << 6;
            mgArr = ((PLATE - 58) << 6) / mgBvy;
            mgAge = 0;
            mgRes = 0;
            mgBallOn = 1;
            sfx(SFX_SHOT);
            batTop();
        }
        return;
    }
    mgAge++;
    if (!mgRes)
    {
        mgBx += mgBvx;
        mgBy += mgBvy;
        if ((pressed & KEY_A) && !mgSwing)
        {
            mgSwing = 12;
            sfx(SFX_SWING);
            d = (s16)mgAge - mgArr;
            ad = ABS(d);
            mgRes = ad <= 2 ? 1 : ad <= 5 ? 2 : ad <= 8 ? 3 : 4; /* home run, hit, foul, miss */
            if (mgRes == 1)
            {
                mgHr++;
                mgHits++;
                mgBvy = -384;
                mgBvx = (s16)(rnd() & 127) - 64;
                sfx(SFX_JING);
                setMsg("HOME RUN!");
            }
            else if (mgRes == 2)
            {
                mgHits++;
                mgBvy = -256;
                mgBvx = (d < 0 ? -1 : 1) * (96 + (rnd() & 127));
                sfx(SFX_HIT);
                setMsg("Base hit!");
            }
            else if (mgRes == 3)
            {
                mgBvy = -160;
                mgBvx = d < 0 ? -192 : 192;
                sfx(SFX_CLANK);
                setMsg(d < 0 ? "Foul, too early" : "Foul, too late");
            }
            else
                setMsg(d < 0 ? "Swing and a miss! Too early." : "Swing and a miss! Too late.");
            batTop();
        }
        if ((mgBy >> 6) > PLATE + 18 && !mgRes)
        {
            mgRes = 4;
            setMsg("Strike! You watched it go by.");
        }
    }
    else
    {
        mgBx += mgBvx;
        mgBy += mgBvy;
        if (mgRes != 4) mgBvy += 3;
    }
    if (mgRes && ((mgBy >> 6) < -10 || (mgBy >> 6) > 210 || mgAge > mgArr + 70))
    {
        mgBallOn = 0;
        mgN++;
        mgWait = 50;
        batTop();
    }
}

static void batDraw(void)
{
    u8 s;
    s16 y;
    if (mgBallOn)
    {
        y = mgBy >> 6;
        s = y < 0 ? 2 : 2 + (y + 30) / 60;
        if (s > 5) s = 5;
        put8(0, (mgBx >> 6) - (s >> 1), y - (s >> 1) + MY, MT_BALL2 + s - 2, 1);
    }
    else sprHide(OAM_M);
    stick(1, 150 - 4, PLATE - 6 + MY, batA[mgSwing], 4, 16, 13, MT_BAT, MT_BATTIP);
}

/* ---------- Rams free throws (web MINI.hoops) ---------- */
static void hoopsTop(void)
{
    char *d = putStr(top, "Shot ");
    d = putNum(d, mgN < 10 ? mgN + 1 : 10);
    d = putStr(d, "/10   Made ");
    putNum(d, mgMade);
    topDirty = 1;
}

static s16 needle(void)
{
    return (sinTab[(mgPh >> 8) & 63] * 50) >> 6;
}

static void hoopsUpdate(u16 pressed)
{
    s16 off;
    u8 ad;
    if (mgBallOn)
    {
        mgBt++;
        if (mgBt == 40)
        {
            if (mgRes != 3)
            {
                mgMade++;
                sfx(SFX_JING);
            }
            else sfx(SFX_CLANK);
            setMsg(mgRes == 1 ? "Swish!" : mgRes == 2 ? "In off the rim!" : "Clank. Missed.");
            hoopsTop();
        }
        if (mgBt > 70)
        {
            mgBallOn = 0;
            mgN++;
            hoopsTop();
        }
        return;
    }
    if (mgN >= 10)
    {
        mgDone = 1;
        return;
    }
    mgPh += 130 + mgN * 21; /* web sp .05 + n * .008 a frame */
    if (pressed & KEY_A)
    {
        off = needle();
        ad = ABS(off);
        mgRes = ad < 7 ? 1 : ad < 15 ? ((rnd() % 10) < 6 ? 2 : 3) : 3; /* swish, rim, miss */
        mgSx0 = 128;
        mgSx1 = 128 + (mgRes == 3 ? (off < 0 ? -14 : 14) : 0);
        mgBt = 0;
        mgBallOn = 1;
        sfx(SFX_SHOT);
    }
}

static void hoopsDraw(void)
{
    s16 x, y;
    if (mgBallOn)
    {
        x = mgSx0 + ((mgSx1 - mgSx0) * (s16)(mgBt > 40 ? 40 : mgBt)) / 40;
        y = 150 + ((56 - 150) * (s16)(mgBt > 40 ? 40 : mgBt)) / 40 - ((sinTab[((mgBt > 40 ? 40 : mgBt) * 32 / 40) & 63] * 70) >> 6);
        if (mgBt > 40) y += (mgBt - 40) * 2; /* drops through */
        put8(0, x - 3, y - 3 + MY, MT_BBALL, 1);
        sprHide(OAM_M + 1);
        sprHide(OAM_M + 2);
    }
    else
    {
        sprHide(OAM_M);
        x = 128 + needle() - 1;
        put8(1, x, 93 + MY, MT_NEEDLE, 1);
        put8(2, x, 101 + MY, MT_NEEDLE, 1);
    }
}

/* ---------- tennis at the Ashe Center (web MINI.tennis) ---------- */
static const u8 tenA[15] = {42, 13, 10, 7, 4, 1, 62, 59, 56, 53, 50, 47, 44, 41, 38};

static void tennisTop(void)
{
    char *d = putStr(top, "Returns ");
    d = putNum(d, mgRet);
    d = putStr(d, "   Misses ");
    d = putNum(d, mgMiss);
    putStr(d, "/3");
    topDirty = 1;
}

static void tennisUpdate(u16 pad, u16 pressed)
{
    s16 x, y, sp, tx;
    if (pad & KEY_LEFT) mgHx -= 35;
    if (pad & KEY_RIGHT) mgHx += 35;
    if (mgHx < 40 * 16) mgHx = 40 * 16;
    if (mgHx > 216 * 16) mgHx = 216 * 16;
    if (mgSwing) mgSwing--;
    if ((pressed & KEY_A) && !mgSwing)
    {
        mgSwing = 14;
        sfx(SFX_SWING);
    }
    if (mgMiss >= 3)
    {
        mgDone = 1;
        return;
    }
    if (!mgBallOn)
    {
        if (!--mgWait)
        {
            sp = 102 + (mgRet * 6 > 141 ? 141 : mgRet * 6); /* 1.6 + min(2.2, ret * .09) px a frame */
            tx = 50 + rnd() % 156;
            mgBx = 128 << 6;
            mgBy = 40 << 6;
            mgBvx = ((tx - 128) * sp) / 110;
            mgBvy = sp;
            mgBallUp = 0;
            mgBallOn = 1;
            sfx(SFX_SHOT);
        }
        return;
    }
    mgBx += mgBvx;
    mgBy += mgBvy;
    x = mgBx >> 6;
    y = mgBy >> 6;
    if (x < 30 || x > 226) mgBvx = -mgBvx;
    if (!mgBallUp && mgSwing > 6 && ABS(y - 150) < 12 && ABS(x - (mgHx >> 4)) < 18)
    {
        mgBallUp = 1;
        mgBvy = -205;
        mgBvx = (x - (mgHx >> 4)) * 8;
        mgRet++;
        sfx(SFX_HIT);
        {
            char *d = putStr(msg, "Return ");
            d = putNum(d, mgRet);
            putStr(d, "!");
            msgDirty = 1;
        }
        tennisTop();
    }
    if (mgBallUp && y < 34)
    {
        mgBallOn = 0;
        mgWait = 30;
    }
    if (!mgBallUp && y > 180)
    {
        mgBallOn = 0;
        mgWait = 50;
        mgMiss++;
        sfx(SFX_HURT);
        {
            char *d = putStr(msg, "Missed! (");
            d = putNum(d, 3 - mgMiss);
            putStr(d, " left)");
            msgDirty = 1;
        }
        tennisTop();
    }
}

static void tennisDraw(void)
{
    s16 x = mgHx >> 4;
    if (mgBallOn) put8(0, (mgBx >> 6) - 2, (mgBy >> 6) - 2 + MY, MT_TBALL, 1);
    else sprHide(OAM_M);
    sprPut(OAM_HERO, 1, x - 8, 142 + MY, 2, 0, 4, 0); /* the hero from behind */
    stick(1, x + 4, 150 + MY, tenA[mgSwing], 4, 13, 9, MT_RACKET, MT_RACKETHEAD);
}

/* ---------- the soapbox derby, the Capital Trail, the Raceway (web derbyUpdate, drawDerby/drawTrail/drawRace) ---------- */
#define MAX_OBS 64
extern void rcqLoadRace(u8 k);
u8 rcK;                        /* course: 0 derby, 1 trail, 2 raceway */
u16 rcY, rcV, rcTime, rcLen;   /* distance (px), speed (1/256 px a frame), frames */
u8 rcFrac, rcBump, rcN, rcMedal;
s16 rcX;                       /* 1/16 px */
u16 obsY[MAX_OBS];
u8 obsX[MAX_OBS], obsK[MAX_OBS], obsV[MAX_OBS], obsF[MAX_OBS], obsHit[MAX_OBS];
static u16 rcShown;

static char *putTime(char *d, u16 f)
{
    u16 tt = f / 6, sec = tt / 10;
    d = putNum(d, sec / 60);
    *d++ = ':';
    *d++ = '0' + (sec % 60) / 10;
    *d++ = '0' + (sec % 60) % 10;
    *d++ = '.';
    *d++ = '0' + tt % 10;
    *d = 0;
    return d;
}

static void raceTop(void)
{
    char *d = putTime(top, rcTime);
    d = putStr(d, "      ");
    putStr(d, raceLabel[rcK]);
    topDirty = 1;
}

static void raceStart(void)
{
    u16 y;
    u8 k;
    rcK = mgKind - MINI_DERBY;
    rcLen = raceLen[rcK];
    rcY = rcV = rcTime = 0;
    rcFrac = rcBump = 0;
    rcX = 128 * 16;
    rcN = 0;
    for (y = 260; y < rcLen - 100 && rcN < MAX_OBS; y += raceStep[rcK] + rnd() % 70)
    {
        k = raceKinds[rcK * 4 + (rnd() & 3)];
        obsY[rcN] = y;
        obsX[rcN] = 76 + rnd() % 100;
        obsK[rcN] = k;
        obsV[rcN] = (k & 0x80) ? 77 + rnd() % 90 : 0; /* a car: 1.2-2.6 px a frame, in 1/64 */
        obsF[rcN] = 0;
        obsHit[rcN] = 0;
        rcN++;
    }
    rcShown = 0xFFFF;
}

static void raceUpdate(u16 pad)
{
    u8 i, k;
    u16 vmax;
    s16 sy, dx;
    rcTime++;
    vmax = rcBump ? 410 : rcK == 2 ? 1331 : 1075;   /* 1.6 while bumped, else 5.2 (raceway) or 4.2 */
    if (rcBump) rcBump--;
    rcV += rcK == 2 ? 13 : 10;                     /* .05 or .04 a frame */
    if (rcV > vmax) rcV = vmax;
    rcFrac += rcV & 255;
    rcY += (rcV >> 8) + (rcFrac < (rcV & 255) ? 1 : 0);
    if (pad & KEY_LEFT) rcX -= 29;
    if (pad & KEY_RIGHT) rcX += 29;
    if (rcX < 64 * 16) rcX = 64 * 16;
    if (rcX > 192 * 16) rcX = 192 * 16;
    for (i = 0; i < rcN; i++)
    {
        if (obsV[i])
        {
            k = obsF[i];
            obsF[i] += obsV[i];
            obsY[i] += obsF[i] >> 6;
            obsF[i] &= 63;
            (void)k;
        }
        if (obsHit[i]) continue;
        sy = (s16)(obsY[i] - rcY);
        if (sy <= 120 || sy >= 150) continue;
        dx = (s16)obsX[i] - (rcX >> 4);
        if (ABS(dx) >= 14) continue;
        obsHit[i] = 1;
        if (obsK[i] & 0x40)
        {
            rcV = (rcV * 3) / 5; /* a puddle or oil: slide, slower */
            sfx(SFX_HEART);
        }
        else
        {
            rcBump = 40;
            rcV = 256;
            sfx(SFX_HURT);
        }
    }
    if (rcTime / 6 != rcShown)
    {
        rcShown = rcTime / 6;
        raceTop();
    }
    if (rcY >= rcLen)
    {
        rcMedal = rcTime < raceGold[rcK] ? 0 : rcTime < raceSilver[rcK] ? 1 : 2;
        mgDone = 1;
    }
}

static void raceDraw(void)
{
    u8 i, c, slot = 0, o, pal;
    s16 sy, sx, cy;
    u16 t;
    bgSetScroll(0, 0, rcY & 511);
    /* the player, bouncing a pixel while bumped */
    cy = 138 + MY + ((rcBump && (mgT & 2)) ? 1 : 0);
    sx = (rcX >> 4) - 16;
    for (c = 0; c < 4; c++)
    {
        t = raceTile[rcK * 16 + c];
        if (t) sprPut(OAM_M + slot++, 1, sx + (c & 1) * 16, cy - 18 + (c >> 1) * 16, 3, 0, t, racePal[rcK * 4]);
    }
    /* what's on the road ahead */
    for (i = 0; i < rcN && slot < 36; i++)
    {
        sy = (s16)(obsY[i] - rcY);
        if (sy < -40 || sy > 230) continue;
        o = obsK[i] & 3;
        pal = racePal[rcK * 4 + o];
        for (c = 0; c < 4; c++)
        {
            t = raceTile[rcK * 16 + o * 4 + c];
            if (t) sprPut(OAM_M + slot++, 1, obsX[i] - 16 + (c & 1) * 16, sy + MY - 16 + (c >> 1) * 16, 2, 0, t, pal);
        }
    }
    while (slot < 36) sprHide(OAM_M + slot++);
    /* how far down the course: a marker up the right-hand edge */
    put8(40, 245, 30 + MY + 138 - ((rcY >> 4) * 138) / (rcLen >> 4), MT_NEEDLE, 1);
}

/* Called from rcqVBlank straight after the VBlank starts: a changed score line or message goes to BG3 then
   (after the sound driver's turn it would land outside VBlank and be lost). */
void miniVBlank(void)
{
    /* only the copy happens here: the lines are laid out in the main loop */
    if (topReady)
    {
        dmaCopyVram((u8 *)rowTop, VRAM_BG3_MAP + albumRow * 32, 64);
        topReady = 0;
    }
    else if (msgReady)
    {
        dmaCopyVram((u8 *)rowMsg, VRAM_BG3_MAP + 27 * 32, 64);
        msgReady = 0;
    }
}

/* ---------- Stone House pinball (web pinUpdate, segHit): B the left flipper, A the right ---------- */
s16 pbX, pbY, pbVX, pbVY;      /* 1/64 px */
u8 pbOn, pbBalls, pbLaunch, pbStill;
u16 pbScore;
u8 pbFl[2];                    /* flippers, eighths of the way up */
s8 pbFlv[2];
u8 pbLit[5];
static const u8 bumpX[5] = {92, 164, 128, 60, 196}, bumpY[5] = {60, 60, 92, 104, 104}, bumpR[5] = {9, 9, 10, 7, 7};

static u8 isqrt(u16 n)
{
    u8 r = 0, b;
    for (b = 128; b; b >>= 1)
        if ((u16)(r | b) * (r | b) <= n) r |= b;
    return r;
}

static void pinTop(void)
{
    char *d = putStr(top, "SCORE ");
    d = putNum(d, pbScore);
    d = putStr(d, "    BALLS ");
    putNum(d, pbBalls);
    topDirty = 1;
}

/* the ball against a segment from (x1, y1) along (ux, uy)/64 for L px, 3 px thick (web segHit); boost: a flipper going up */
static void pinSeg(s16 x1, s16 y1, s8 ux, s8 uy, u8 L, s8 boost, s8 side)
{
    s16 rx, ry, along, ex, ey, nx, ny, dot;
    u8 d;
    rx = pbX - (x1 << 6);
    ry = pbY - (y1 << 6);
    if (ABS(rx) > (L + 6) * 64 || ABS(ry) > (L + 6) * 64) return;
    along = ((rx >> 6) * ux + (ry >> 6) * uy) >> 6;
    if (along < 0) along = 0;
    if (along > L) along = L;
    ex = rx - ux * along;
    ey = ry - uy * along;
    if (ABS(ex) >= 3 * 64 || ABS(ey) >= 3 * 64) return;
    d = isqrt((u16)((ex >> 3) * (ex >> 3) + (ey >> 3) * (ey >> 3)));
    if (d >= 24 || !d) return;
    nx = ((ex >> 3) << 6) / d;
    ny = ((ey >> 3) << 6) / d;
    pbX = (x1 << 6) + ux * along + nx * 3;
    pbY = (y1 << 6) + uy * along + ny * 3;
    dot = ((pbVX * nx) >> 6) + ((pbVY * ny) >> 6);
    if (dot > 0) return;
    pbVX = ((pbVX - ((dot * nx) >> 5)) * 27) >> 5;
    pbVY = ((pbVY - ((dot * ny) >> 5)) * 27) >> 5;
    if (boost > 0)
    {
        pbVY -= 40 * boost + 128;
        pbVX += side * 77;
    }
}

static void pinStep(u8 q)
{
    u8 i, r;
    s16 dx, dy, nx, ny, dot, sx, sy;
    u8 d, s;
    /* quarter steps (the web game takes halves): a fast ball can't slip through a 3 px guide */
    if (!(q & 1)) pbVY += 3;
    pbX += pbVX >> 2;
    pbY += pbVY >> 2;
    sx = pbVX >> 3;
    sy = pbVY >> 3;
    if (ABS(sx) > 40 || ABS(sy) > 40 || sx * sx + sy * sy > 3136)
    {
        s = isqrt((u16)(ABS(sx) > 120 ? 14400 : sx * sx) + (u16)(ABS(sy) > 120 ? 14400 : sy * sy));
        if (s > 56)
        {
            pbVX = ((sx * 56) / s) << 3;
            pbVY = ((sy * 56) / s) << 3;
        }
    }
    if (pbX < 22 * 64)
    {
        pbX = 22 * 64;
        pbVX = (ABS(pbVX) * 4) / 5;
    }
    if (pbX > 244 * 64)
    {
        pbX = 244 * 64;
        pbVX = -(ABS(pbVX) * 4) / 5;
    }
    if (pbY < 34 * 64)
    {
        pbY = 34 * 64;
        pbVY = (ABS(pbVY) * 4) / 5;
    }
    dx = pbX >> 6;
    dy = pbY >> 6;
    if (dy < 70 && dx > 200 && (dx - 200) + (70 - dy) > 60) pbVX = -ABS(pbVX) - 32;
    for (i = 0; i < 5; i++)
    {
        r = bumpR[i];
        dx = (pbX >> 6) - bumpX[i];
        dy = (pbY >> 6) - bumpY[i];
        if (ABS(dx) >= r + 3 || ABS(dy) >= r + 3) continue;
        if (dx * dx + dy * dy >= (r + 3) * (r + 3)) continue;
        d = isqrt((u16)(dx * dx + dy * dy));
        if (!d)
        {
            d = 1;
            dy = -1;
        }
        nx = (dx << 6) / d;
        ny = (dy << 6) / d;
        pbX = (bumpX[i] << 6) + nx * (r + 3);
        pbY = (bumpY[i] << 6) + ny * (r + 3);
        dot = ((pbVX * nx) >> 6) + ((pbVY * ny) >> 6);
        pbVX -= (dot * nx) >> 5;
        pbVY -= (dot * ny) >> 5;
        pbVX += (nx * 102) >> 6;
        pbVY += (ny * 102) >> 6;
        pbScore += 100;
        pbLit[i] = 12;
        sfx(SFX_COIN);
        pinTop();
    }
    pinSeg(22, 150, 60, 23, 79, 0, 0);    /* the guides down to the flippers */
    pinSeg(244, 150, -61, 20, 89, 0, 0);
    pinSeg(96, 178, pinUX[pbFl[0]], pinUY[pbFl[0]], 26, pbFlv[0], 1);
    pinSeg(160, 178, pinUX[9 + pbFl[1]], pinUY[9 + pbFl[1]], 26, pbFlv[1], -1);
}

static void pinUpdate(u16 pad)
{
    u8 i, on, prev;
    for (i = 0; i < 2; i++)
    {
        on = (pad & (i ? KEY_A : KEY_B)) != 0;
        prev = pbFl[i];
        if (on) pbFl[i] = pbFl[i] + 3 > 8 ? 8 : pbFl[i] + 3;
        else pbFl[i] = pbFl[i] > ((mgT & 1) ? 1 : 2) ? pbFl[i] - ((mgT & 1) ? 1 : 2) : 0;
        pbFlv[i] = (s8)pbFl[i] - (s8)prev;
        if (on && !prev) sfx(SFX_MENU);
    }
    for (i = 0; i < 5; i++)
        if (pbLit[i]) pbLit[i]--;
    if (!pbOn)
    {
        if (!pbBalls)
        {
            mgDone = 1;
            return;
        }
        if (!--pbLaunch)
        {
            pbX = 236 << 6;
            pbY = 40 << 6;
            pbVX = -102 - (rnd() & 63);
            pbVY = 32;
            pbOn = 1;
            pbStill = 0;
            sfx(SFX_SHOT);
            {
                char *d = putStr(msg, "Ball ");
                d = putNum(d, 4 - pbBalls);
                putStr(d, " of 3");
                msgDirty = 1;
            }
        }
        return;
    }
    for (i = 0; i < 4; i++) pinStep(i);
    if (ABS(pbVX) + ABS(pbVY) < 22)
    {
        if (++pbStill > 40)
        {
            pbVX += (pbX >> 6) < 128 ? 58 : -58;
            pbVY -= 38;
            pbStill = 0;
        }
    }
    else pbStill = 0;
    if ((pbY >> 6) > 206)
    {
        pbOn = 0;
        pbBalls--;
        pbLaunch = 60;
        sfx(SFX_FAIL);
        setMsg(pbBalls ? "Ball lost!" : "");
        pinTop();
    }
}

static void pinDraw(void)
{
    u8 i, r, slot = 1, c, z;
    s16 x, y;
    if (pbOn) put8(0, (pbX >> 6) - 3, (pbY >> 6) - 3 + MY, MT_PINBALL, 1);
    else sprHide(OAM_M);
    for (i = 0; i < 2; i++)
        for (r = 0; r <= 24; r += 3)
        {
            x = (i ? 160 : 96) + ((pinUX[i * 9 + pbFl[i]] * r) >> 6);
            y = 178 + ((pinUY[i * 9 + pbFl[i]] * r) >> 6);
            put8(slot++, x - 2, y - 2 + MY, MT_PINDOT, 1);
        }
    for (i = 0; i < 5; i++)
    {
        z = bumpR[i] == 9 ? 0 : bumpR[i] == 10 ? 1 : 2;
        for (c = 0; c < 4; c++, slot++)
            if (pbLit[i]) sprPut(OAM_M + slot, 1, bumpX[i] - 16 + (c & 1) * 16, bumpY[i] - 16 + MY + (c >> 1) * 16, 2, 0, pinLitTile[z * 4 + c], 2);
            else sprHide(OAM_M + slot);
    }
}

/* ---------- the photo album (web drawAlbum): every landmark, the ones you haven't taken as a ? ---------- */
static u16 albumMap[32 * 28];
extern u8 photoGot[];
u8 photoCount(void);
static u8 albumCur;

static void albumCaption(void)
{
    u8 k = albumCur;
    if (photoGot[k >> 3] & (1 << (k & 7)))
    {
        bg3Fill(rowTop, photoCap1[k], FONT_GOLD);
        bg3Fill(rowMsg, photoCap2[k], FONT_GOLD);
    }
    else
    {
        bg3Fill(rowTop, "Not taken yet.", FONT_WHITE);
        bg3Fill(rowMsg, "Use the camera near it.", FONT_WHITE);
    }
    topReady = msgReady = 1;
}

static void albumCursor(void)
{
    s16 x = 8 + (albumCur % 5) * 48, y = 16 + (albumCur / 5) * 32;
    put8(0, x - 2, y - 2, MT_CORNERTL, 1);
    put8(1, x + 42, y - 2, MT_CORNERTR, 1);
    put8(2, x - 2, y + 26, MT_CORNERBL, 1);
    put8(3, x + 42, y + 26, MT_CORNERBR, 1);
}

void runAlbum(void)
{
    u16 pad, pressed, lastPad = 0xFFFF, i;
    u8 k, r, c, n;
    char title[24], *d;
    mgKind = 0;
    topDirty = msgDirty = topReady = msgReady = 0;
    top[0] = msg[0] = 0;
    miniOn = 1;
    albumRow = 26;
    fadeTo(0);
    setBrightness(0);
    closeText();
    flushText();
    for (k = 0; k < 128; k++) sprHide(k);
    dmaCopyVram(&album_chr, VRAM_BG1_CHR, (&album_chrend - &album_chr));
    dmaCopyCGram(&album_pal, 0, 8 * 16 * 2);
    /* the photos you haven't taken yet are a blank block with a question mark */
    for (i = 0; i < 32 * 28; i++) albumMap[i] = ((u16 *)&album_map)[i];
    for (k = 0; k < PHOTO_COUNT; k++)
        if (!(photoGot[k >> 3] & (1 << (k & 7))))
            for (r = 0; r < 4; r++)
                for (c = 0; c < 6; c++)
                    albumMap[(2 + (k / 5) * 4 + r) * 32 + 1 + (k % 5) * 6 + c] = albumBlank[r * 6 + c];
    dmaCopyVram((u8 *)albumMap, VRAM_BG1_MAP, 32 * 28 * 2);
    dmaCopyVram(&minispr_chr, VRAM_OBJ_CHR + 32 * 16, (&minispr_chrend - &minispr_chr));
    dmaCopyCGram(&minispr_pal, 128 + 16, 32);
    bgSetScroll(0, 0, 0);
    REG_HDMAEN = 0;
    REG_CGADSUB = 0;
    n = photoCount();
    d = putStr(title, "PHOTOS ");
    d = putNum(d, n);
    d = putStr(d, "/");
    putNum(d, PHOTO_COUNT);
    bg3Row(0, title, FONT_GOLD);
    albumCur = 0;
    albumCaption();
    bg3Row(26, "", FONT_WHITE);
    bg3Row(27, "", FONT_WHITE);
    albumCursor();
    fadeTo(1);
    padSeen = 0;
    while (1)
    {
        pad = padsCurrent(0);
        pressed = (pad | padSeen) & ~lastPad;
        padSeen = 0;
        lastPad = pad;
        if (pressed & (KEY_B | KEY_START)) break;
        k = albumCur;
        if ((pressed & KEY_LEFT) && albumCur % 5) albumCur--;
        if ((pressed & KEY_RIGHT) && albumCur % 5 < 4 && albumCur + 1 < PHOTO_COUNT) albumCur++;
        if ((pressed & KEY_UP) && albumCur >= 5) albumCur -= 5;
        if ((pressed & KEY_DOWN) && albumCur + 5 < PHOTO_COUNT) albumCur += 5;
        if (k != albumCur)
        {
            sfx(SFX_MENU);
            albumCaption();
            albumCursor();
        }
        WaitForVBlank();
    }
    fadeTo(0);
    setBrightness(0);
    for (k = 0; k < 128; k++) sprHide(k);
    bg3Row(0, "", FONT_WHITE);
    bg3Row(26, "", FONT_WHITE);
    bg3Row(27, "", FONT_WHITE);
    albumRow = 0;
    miniOn = 0;
    lightDirty = 1;
    enterMap(curMap);
}

/* ---------- the frame around them ---------- */
static const char *const miniName[8] = {"", "Batting Practice", "Rams Free Throws", "Ashe Center Tennis", "", "", "", "Stone House Pinball"};
static const char *const medalName[3] = {"gold", "silver", "bronze"};
static const u8 medalCoins[3] = {20, 10, 5};
static u8 endTxt[200];

static void fadeTo(u8 in)
{
    u8 b;
    for (b = 1; b <= 15; b++)
    {
        setBrightness(in ? b : 15 - b);
        WaitForVBlank();
    }
}

/* the result, as a text entry: title NUL, lines split by 0x0A, pages by 0x0C, NUL */
static void endText(void)
{
    char *d = (char *)endTxt;
    u16 coinsWon = 0, score = 0;
    d = putStr(d, mgKind >= MINI_DERBY && mgKind != MINI_PIN ? raceName[mgKind - MINI_DERBY] : miniName[mgKind]) + 1;
    if (mgQuit)
        d = putStr(d, "You walk away from the game.");
    else if (mgKind == MINI_BAT)
    {
        coinsWon = mgHits * 2 + mgHr * 5;
        score = mgHits + mgHr;
        d = putStr(d, "Ten pitches: ");
        d = putNum(d, mgHits);
        d = putStr(d, " hits,\n");
        d = putNum(d, mgHr);
        d = putStr(d, " home runs.\fThe coach slips you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
    }
    else if (mgKind == MINI_PIN)
    {
        coinsWon = pbScore / 500;
        score = pbScore;
        d = putStr(d, "Game over! Score: ");
        d = putNum(d, pbScore);
        d = putStr(d, ".\fThe machine spits out\n");
        d = putNum(d, coinsWon);
        d = putStr(d, coinsWon == 1 ? " coin." : " coins.");
    }
    else if (mgKind >= MINI_DERBY)
    {
        /* lower is better: keep the best time */
        coinsWon = medalCoins[rcMedal];
        d = putStr(d, "Finished in ");
        d = putTime(d, rcTime);
        d = putStr(d, "!\nThat's a ");
        d = putStr(d, medalName[rcMedal]);
        d = putStr(d, " medal.\f");
        d = putStr(d, raceWho[rcK]);
        d = putStr(d, " hands\nover ");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
        if (!miniBest[mgKind] || rcTime < miniBest[mgKind])
        {
            miniBest[mgKind] = rcTime;
            d = putStr(d, "\nA new best time!");
        }
    }
    else if (mgKind == MINI_HOOPS)
    {
        coinsWon = mgMade * 2;
        score = mgMade;
        d = putNum(d, mgMade);
        d = putStr(d, " of 10 from the line.\fThe coach tosses you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
        if (mgMade >= 8) d = putStr(d, "\nThat's Havoc!");
    }
    else
    {
        coinsWon = mgRet;
        score = mgRet;
        d = putNum(d, mgRet);
        d = putStr(d, " returns before\nthree misses.\fThe coach hands you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
        if (mgRet >= 15) d = putStr(d, "\nArthur Ashe would be proud.");
    }
    if (!mgQuit && (mgKind < MINI_DERBY || mgKind == MINI_PIN) && score > miniBest[mgKind])
    {
        miniBest[mgKind] = score;
        d = putStr(d, "\nYour best yet!");
    }
    coins = coins + coinsWon > 999 ? 999 : coins + coinsWon;
    d[1] = 0;
}

void runMini(u8 k)
{
    u16 pad, pressed, lastPad = 0xFFFF;
    u8 i;
    mgKind = k;
    topDirty = msgDirty = topReady = msgReady = 0; /* (work RAM starts as noise) */
    top[0] = msg[0] = 0;
    miniOn = 1;
    fadeTo(0);
    setBrightness(0);
    closeText();
    flushText(); /* the question box off BG3 */
    for (i = 0; i < 128; i++) sprHide(i);
    if (k >= MINI_DERBY && k != MINI_PIN) rcqLoadRace(k - MINI_DERBY);
    else rcqLoadMini(k - 1);
    if (k == MINI_PIN)
    {
        dmaCopyVram(&pinlit_chr, VRAM_OBJ_CHR + 64 * 16, (&pinlit_chrend - &pinlit_chr));
        dmaCopyCGram(&pinlit_pal, 128 + 32, 32);
    }
    dmaCopyVram(&minispr_chr, VRAM_OBJ_CHR + 32 * 16, (&minispr_chrend - &minispr_chr));
    dmaCopyCGram(&minispr_pal, 128 + 16, 32);
    bgSetScroll(0, 0, 0);
    REG_HDMAEN = 0; /* no night or weather over the game */
    REG_CGADSUB = 0;
    mgDone = mgQuit = 0;
    mgN = mgHits = mgHr = mgMade = mgRet = mgMiss = mgSwing = mgBallOn = 0;
    mgWait = 60;
    mgPh = 0;
    mgHx = 128 * 16;
    msg[0] = 0;
    if (k == MINI_BAT)
    {
        batTop();
        setMsg("A: swing   START: quit");
    }
    else if (k == MINI_HOOPS)
    {
        hoopsTop();
        setMsg("A: shoot   START: quit");
    }
    else if (k == MINI_TENNIS)
    {
        tennisTop();
        setMsg("A: swing   START: quit");
    }
    else if (k == MINI_PIN)
    {
        pbOn = 0;
        pbBalls = 3;
        pbLaunch = 40;
        pbScore = 0;
        pbFl[0] = pbFl[1] = 0;
        for (i = 0; i < 5; i++) pbLit[i] = 0;
        pinTop();
        setMsg("B: left flipper  A: right");
    }
    else
    {
        raceStart();
        raceTop();
        setMsg("Steer with left and right");
    }
    WaitForVBlank();
    bg3Row(0, top, FONT_GOLD);
    bg3Row(27, msg, FONT_WHITE);
    fadeTo(1);
    padSeen = 0;
    while (!mgDone)
    {
        pad = padsCurrent(0);
        pressed = (pad | padSeen) & ~lastPad;
        padSeen = 0;
        lastPad = pad;
        if (pressed & KEY_START)
        {
            mgQuit = 1;
            break;
        }
        if (mgKind == MINI_BAT)
        {
            batUpdate(pressed);
            batDraw();
        }
        else if (mgKind == MINI_HOOPS)
        {
            hoopsUpdate(pressed);
            hoopsDraw();
        }
        else if (mgKind == MINI_TENNIS)
        {
            tennisUpdate(pad, pressed);
            tennisDraw();
        }
        else if (mgKind == MINI_PIN)
        {
            pinUpdate(pad);
            pinDraw();
        }
        else
        {
            raceUpdate(pad);
            raceDraw();
        }
        mgT++;
        if (topDirty && !topReady)
        {
            bg3Fill(rowTop, top, FONT_GOLD);
            topDirty = 0;
            topReady = 1;
        }
        if (msgDirty && !msgReady)
        {
            bg3Fill(rowMsg, msg, FONT_WHITE);
            msgDirty = 0;
            msgReady = 1;
        }
        WaitForVBlank();
    }
    /* the result, then back to the neighbourhood */
    for (i = 0; i < 40; i++) WaitForVBlank();
    fadeTo(0);
    setBrightness(0);
    for (i = 0; i < 128; i++) sprHide(i);
    bgSetScroll(0, 0, 0);
    bg3Row(0, "", FONT_WHITE);
    bg3Row(27, "", FONT_WHITE);
    miniOn = 0;
    lightDirty = 1;
    endText();
    enterMap(curMap);
    openTextAt(endTxt);
}

/* BG1 tiles, the top-left screen of its map and its palettes, in forced blank */
void rcqMiniUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal)
{
    dmaCopyVram(chr, VRAM_BG1_CHR, chrSize);
    dmaCopyVram(map, VRAM_BG1_MAP, 2048);
    dmaCopyCGram(pal, 0, 8 * 16 * 2);
}

/* a race course: BG1 tiles, its whole 64x64 map (it loops as it scrolls), palettes, and its sprites at tile 64 */
void rcqRaceUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *och, u16 ochSize, u8 *opl)
{
    dmaCopyVram(chr, VRAM_BG1_CHR, chrSize);
    dmaCopyVram(map, VRAM_BG1_MAP, 64 * 64 * 2);
    dmaCopyCGram(pal, 0, 8 * 16 * 2);
    dmaCopyVram(och, VRAM_OBJ_CHR + 64 * 16, ochSize);
    dmaCopyCGram(opl, 128 + 32, 3 * 32);
}
