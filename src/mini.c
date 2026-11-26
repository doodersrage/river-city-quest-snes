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
static u16 t;
/* shared game state */
u8 mgN, mgHits, mgHr, mgMade, mgRet, mgMiss, mgWait, mgSwing, mgRes;
u8 mgBallOn, mgBallUp, mgAge, mgArr;
s16 mgBx, mgBy, mgBvx, mgBvy;  /* ball, 1/64 px */
s16 mgHx;                /* the tennis player, 1/16 px */
u16 mgPh;                /* free-throw needle phase */
u8 mgSx0, mgSx1, mgBt;       /* free-throw flight: start/end x, frames */
static char msg[33], top[33];
static u8 msgDirty, topDirty;
static u16 row[32];

/* ---------- text on BG3 rows 0 (score) and 27 (messages), outside the picture ---------- */
static void bg3Row(u8 r, const char *s, u16 font)
{
    u8 c, k = 0;
    while (s[k]) k++;
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

/* ---------- the frame around them ---------- */
static const char *const miniName[4] = {"", "Batting Practice", "Rams Free Throws", "Ashe Center Tennis"};
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
    d = putStr(d, miniName[mgKind]) + 1;
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
    if (!mgQuit && score > miniBest[mgKind])
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
    miniOn = 1;
    fadeTo(0);
    setBrightness(0);
    closeText();
    flushText(); /* the question box off BG3 */
    for (i = 0; i < 128; i++) sprHide(i);
    rcqLoadMini(k - 1);
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
    else
    {
        tennisTop();
        setMsg("A: swing   START: quit");
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
        else
        {
            tennisUpdate(pad, pressed);
            tennisDraw();
        }
        t++;
        WaitForVBlank();
        if (topDirty)
        {
            bg3Row(0, top, FONT_GOLD);
            topDirty = 0;
        }
        else if (msgDirty)
        {
            bg3Row(27, msg, FONT_WHITE);
            msgDirty = 0;
        }
    }
    /* the result, then back to the neighbourhood */
    for (i = 0; i < 40; i++) WaitForVBlank();
    fadeTo(0);
    setBrightness(0);
    for (i = 0; i < 128; i++) sprHide(i);
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
