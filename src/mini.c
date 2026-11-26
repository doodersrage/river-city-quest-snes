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

u8 miniOn;
u16 miniBest[8];              /* best score per game (higher is better) */

static u8 kind, done, quit;
static u16 t;
/* shared game state */
static u8 n, hits, hr, made, ret, miss, wait, swing, res;
static u8 ballOn, ballUp, age, arr;
static s16 bx, by, bvx, bvy;  /* ball, 1/64 px */
static s16 hx;                /* the tennis player, 1/16 px */
static u16 ph;                /* free-throw needle phase */
static u8 sx0, sx1, bt;       /* free-throw flight: start/end x, frames */
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
    d = putNum(d, n < 10 ? n + 1 : 10);
    d = putStr(d, "/10   Hits ");
    d = putNum(d, hits);
    d = putStr(d, "   HR ");
    putNum(d, hr);
    topDirty = 1;
}

static void batUpdate(u16 pressed)
{
    s16 d;
    u8 ad;
    if (swing) swing--;
    if (!ballOn)
    {
        if (n >= 10)
        {
            done = 1;
            return;
        }
        if (!--wait)
        {
            /* the pitch: 1.6-3.2 px a frame, a little curve */
            bvy = 102 + (rnd() & 63);
            bvx = (s16)(rnd() & 31) - 16;
            bx = 128 << 6;
            by = 58 << 6;
            arr = ((PLATE - 58) << 6) / bvy;
            age = 0;
            res = 0;
            ballOn = 1;
            sfx(SFX_SHOT);
            batTop();
        }
        return;
    }
    age++;
    if (!res)
    {
        bx += bvx;
        by += bvy;
        if ((pressed & KEY_A) && !swing)
        {
            swing = 12;
            sfx(SFX_SWING);
            d = (s16)age - arr;
            ad = ABS(d);
            res = ad <= 2 ? 1 : ad <= 5 ? 2 : ad <= 8 ? 3 : 4; /* home run, hit, foul, miss */
            if (res == 1)
            {
                hr++;
                hits++;
                bvy = -384;
                bvx = (s16)(rnd() & 127) - 64;
                sfx(SFX_JING);
                setMsg("HOME RUN!");
            }
            else if (res == 2)
            {
                hits++;
                bvy = -256;
                bvx = (d < 0 ? -1 : 1) * (96 + (rnd() & 127));
                sfx(SFX_HIT);
                setMsg("Base hit!");
            }
            else if (res == 3)
            {
                bvy = -160;
                bvx = d < 0 ? -192 : 192;
                sfx(SFX_CLANK);
                setMsg(d < 0 ? "Foul, too early" : "Foul, too late");
            }
            else
                setMsg(d < 0 ? "Swing and a miss! Too early." : "Swing and a miss! Too late.");
            batTop();
        }
        if ((by >> 6) > PLATE + 18 && !res)
        {
            res = 4;
            setMsg("Strike! You watched it go by.");
        }
    }
    else
    {
        bx += bvx;
        by += bvy;
        if (res != 4) bvy += 3;
    }
    if (res && ((by >> 6) < -10 || (by >> 6) > 210 || age > arr + 70))
    {
        ballOn = 0;
        n++;
        wait = 50;
        batTop();
    }
}

static void batDraw(void)
{
    u8 s;
    s16 y;
    if (ballOn)
    {
        y = by >> 6;
        s = y < 0 ? 2 : 2 + (y + 30) / 60;
        if (s > 5) s = 5;
        put8(0, (bx >> 6) - (s >> 1), y - (s >> 1) + MY, MT_BALL2 + s - 2, 1);
    }
    else sprHide(OAM_M);
    stick(1, 150 - 4, PLATE - 6 + MY, batA[swing], 4, 16, 13, MT_BAT, MT_BATTIP);
}

/* ---------- Rams free throws (web MINI.hoops) ---------- */
static void hoopsTop(void)
{
    char *d = putStr(top, "Shot ");
    d = putNum(d, n < 10 ? n + 1 : 10);
    d = putStr(d, "/10   Made ");
    putNum(d, made);
    topDirty = 1;
}

static s16 needle(void)
{
    return (sinTab[(ph >> 8) & 63] * 50) >> 6;
}

static void hoopsUpdate(u16 pressed)
{
    s16 off;
    u8 ad;
    if (ballOn)
    {
        bt++;
        if (bt == 40)
        {
            if (res != 3)
            {
                made++;
                sfx(SFX_JING);
            }
            else sfx(SFX_CLANK);
            setMsg(res == 1 ? "Swish!" : res == 2 ? "In off the rim!" : "Clank. Missed.");
            hoopsTop();
        }
        if (bt > 70)
        {
            ballOn = 0;
            n++;
            hoopsTop();
        }
        return;
    }
    if (n >= 10)
    {
        done = 1;
        return;
    }
    ph += 130 + n * 21; /* web sp .05 + n * .008 a frame */
    if (pressed & KEY_A)
    {
        off = needle();
        ad = ABS(off);
        res = ad < 7 ? 1 : ad < 15 ? ((rnd() % 10) < 6 ? 2 : 3) : 3; /* swish, rim, miss */
        sx0 = 128;
        sx1 = 128 + (res == 3 ? (off < 0 ? -14 : 14) : 0);
        bt = 0;
        ballOn = 1;
        sfx(SFX_SHOT);
    }
}

static void hoopsDraw(void)
{
    s16 x, y;
    if (ballOn)
    {
        x = sx0 + ((sx1 - sx0) * (s16)(bt > 40 ? 40 : bt)) / 40;
        y = 150 + ((56 - 150) * (s16)(bt > 40 ? 40 : bt)) / 40 - ((sinTab[((bt > 40 ? 40 : bt) * 32 / 40) & 63] * 70) >> 6);
        if (bt > 40) y += (bt - 40) * 2; /* drops through */
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
    d = putNum(d, ret);
    d = putStr(d, "   Misses ");
    d = putNum(d, miss);
    putStr(d, "/3");
    topDirty = 1;
}

static void tennisUpdate(u16 pad, u16 pressed)
{
    s16 x, y, sp, tx;
    if (pad & KEY_LEFT) hx -= 35;
    if (pad & KEY_RIGHT) hx += 35;
    if (hx < 40 * 16) hx = 40 * 16;
    if (hx > 216 * 16) hx = 216 * 16;
    if (swing) swing--;
    if ((pressed & KEY_A) && !swing)
    {
        swing = 14;
        sfx(SFX_SWING);
    }
    if (miss >= 3)
    {
        done = 1;
        return;
    }
    if (!ballOn)
    {
        if (!--wait)
        {
            sp = 102 + (ret * 6 > 141 ? 141 : ret * 6); /* 1.6 + min(2.2, ret * .09) px a frame */
            tx = 50 + rnd() % 156;
            bx = 128 << 6;
            by = 40 << 6;
            bvx = ((tx - 128) * sp) / 110;
            bvy = sp;
            ballUp = 0;
            ballOn = 1;
            sfx(SFX_SHOT);
        }
        return;
    }
    bx += bvx;
    by += bvy;
    x = bx >> 6;
    y = by >> 6;
    if (x < 30 || x > 226) bvx = -bvx;
    if (!ballUp && swing > 6 && ABS(y - 150) < 12 && ABS(x - (hx >> 4)) < 18)
    {
        ballUp = 1;
        bvy = -205;
        bvx = (x - (hx >> 4)) * 8;
        ret++;
        sfx(SFX_HIT);
        {
            char *d = putStr(msg, "Return ");
            d = putNum(d, ret);
            putStr(d, "!");
            msgDirty = 1;
        }
        tennisTop();
    }
    if (ballUp && y < 34)
    {
        ballOn = 0;
        wait = 30;
    }
    if (!ballUp && y > 180)
    {
        ballOn = 0;
        wait = 50;
        miss++;
        sfx(SFX_HURT);
        {
            char *d = putStr(msg, "Missed! (");
            d = putNum(d, 3 - miss);
            putStr(d, " left)");
            msgDirty = 1;
        }
        tennisTop();
    }
}

static void tennisDraw(void)
{
    s16 x = hx >> 4;
    if (ballOn) put8(0, (bx >> 6) - 2, (by >> 6) - 2 + MY, MT_TBALL, 1);
    else sprHide(OAM_M);
    sprPut(OAM_HERO, 1, x - 8, 142 + MY, 2, 0, 4, 0); /* the hero from behind */
    stick(1, x + 4, 150 + MY, tenA[swing], 4, 13, 9, MT_RACKET, MT_RACKETHEAD);
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
    d = putStr(d, miniName[kind]) + 1;
    if (quit)
        d = putStr(d, "You walk away from the game.");
    else if (kind == MINI_BAT)
    {
        coinsWon = hits * 2 + hr * 5;
        score = hits + hr;
        d = putStr(d, "Ten pitches: ");
        d = putNum(d, hits);
        d = putStr(d, " hits,\n");
        d = putNum(d, hr);
        d = putStr(d, " home runs.\fThe coach slips you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
    }
    else if (kind == MINI_HOOPS)
    {
        coinsWon = made * 2;
        score = made;
        d = putNum(d, made);
        d = putStr(d, " of 10 from the line.\fThe coach tosses you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
        if (made >= 8) d = putStr(d, "\nThat's Havoc!");
    }
    else
    {
        coinsWon = ret;
        score = ret;
        d = putNum(d, ret);
        d = putStr(d, " returns before\nthree misses.\fThe coach hands you\n");
        d = putNum(d, coinsWon);
        d = putStr(d, " coins.");
        if (ret >= 15) d = putStr(d, "\nArthur Ashe would be proud.");
    }
    if (!quit && score > miniBest[kind])
    {
        miniBest[kind] = score;
        d = putStr(d, "\nYour best yet!");
    }
    coins = coins + coinsWon > 999 ? 999 : coins + coinsWon;
    d[1] = 0;
}

void runMini(u8 k)
{
    u16 pad, pressed, lastPad = 0xFFFF;
    u8 i;
    kind = k;
    fadeTo(0);
    setBrightness(0);
    closeText();
    for (i = 0; i < 128; i++) sprHide(i);
    miniOn = 1;
    rcqLoadMini(k - 1);
    dmaCopyVram(&minispr_chr, VRAM_OBJ_CHR + 32 * 16, (&minispr_chrend - &minispr_chr));
    dmaCopyCGram(&minispr_pal, 128 + 16, 32);
    bgSetScroll(0, 0, 0);
    REG_HDMAEN = 0; /* no night or weather over the game */
    REG_CGADSUB = 0;
    done = quit = 0;
    n = hits = hr = made = ret = miss = swing = ballOn = 0;
    wait = 60;
    ph = 0;
    hx = 128 * 16;
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
    while (!done)
    {
        pad = padsCurrent(0);
        pressed = (pad | padSeen) & ~lastPad;
        padSeen = 0;
        lastPad = pad;
        if (pressed & KEY_START)
        {
            quit = 1;
            break;
        }
        if (kind == MINI_BAT)
        {
            batUpdate(pressed);
            batDraw();
        }
        else if (kind == MINI_HOOPS)
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
