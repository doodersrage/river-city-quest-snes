/* River City Quest (SNES): the title screen and name entry (stage 11). The night skyline with the logo, three save
   slots in a panel under it, and below the picture a line of choices. An empty slot starts a new game: you name
   your hero (RORY unless you change it) and the opening is read. A used slot continues, or can be erased. */
#include <snes.h>
#include "game.h"

/* this screen does its own VBlank: rows are copied straight after it starts, before the sound driver's turn */
#undef WaitForVBlank

extern u16 padSeen;
extern void rcqLoadMini(u8 k);
extern char heroName[9];
extern u16 playMin;
extern u8 saveSlot, saveBuf[];
u8 slotOk(u8 slot);
void loadGame(u8 slot);
void eraseSlot(u8 slot);

#define ROW_SLOT0 13   /* slots on rows 13, 15, 17, inside the panel */
#define ROW_HINT 27    /* below the picture */
#define MAXQ 5

static u16 rq[MAXQ][32];
static u8 rqRow[MAXQ], rqN;
static char line[33];
static u8 used[3];
static char slotName[3][9];
static u8 slotHearts[3];
static u16 slotMins[3];
static u16 lastPad;

static void tFrame(void)
{
    u8 i;
    WaitForVBlank();
    for (i = 0; i < rqN; i++) dmaCopyVram((u8 *)rq[i], VRAM_BG3_MAP + rqRow[i] * 32, 64);
    rqN = 0;
    spcProcess();
}

/* ---- BG3 rows, copied in the next VBlank ---- */
static u16 *rowBuf(u8 r)
{
    u8 c;
    u16 *b;
    if (rqN >= MAXQ) tFrame();
    b = rq[rqN];
    rqRow[rqN++] = r;
    for (c = 0; c < 32; c++) b[c] = 0;
    return b;
}

static void putAt(u16 *b, u8 c, const char *s, u16 font)
{
    while (*s && c < 32) b[c++] = (font + *s++ - 32) | 0x2000;
}

static u8 len(const char *s)
{
    u8 n = 0;
    while (s[n]) n++;
    return n;
}

static void rowText(u8 r, const char *s, u16 font)
{
    u16 *b = rowBuf(r);
    putAt(b, (32 - len(s)) / 2, s, font);
}


/* new presses since the last look (taps in a long frame still count: src/padnmi.asm) */
static u16 presses(void)
{
    u16 pad = padsCurrent(0), p;
    p = (pad | padSeen) & ~lastPad;
    padSeen = 0;
    lastPad = pad;
    return p;
}

static void fade(u8 in)
{
    u8 b;
    for (b = 1; b <= 15; b++)
    {
        setBrightness(in ? b : 15 - b);
        tFrame();
    }
}

/* the screen's picture (10 the title, 11 the name panel) on BG1, with the BG3 text cleared */
static void loadScreen(u8 k)
{
    u8 i;
    setBrightness(0);
    for (i = 0; i < 128; i++) sprHide(i);
    rcqLoadMini(k);
    bgSetScroll(0, 0, 0);
    REG_HDMAEN = 0;
    REG_CGADSUB = 0;
    rqN = 0;
    for (i = 0; i < 28; i++) rowText(i, "", FONT_WHITE);
    tFrame();
}

/* ---- the slots ---- */
static char *put(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

static char *num(char *d, u16 v)
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

static void readSlots(void)
{
    u8 s, i;
    for (s = 0; s < 3; s++)
    {
        used[s] = slotOk(s + 1);
        if (!used[s]) continue;
        for (i = 0; i < 8; i++) slotName[s][i] = saveBuf[2 + i];
        slotName[s][8] = 0;
        slotMins[s] = saveBuf[11] | (saveBuf[12] << 8);
        slotHearts[s] = saveBuf[13] / 2;
    }
}

static void drawSlot(u8 s, u8 sel)
{
    u16 *b = rowBuf(ROW_SLOT0 + s * 2);
    u16 f = sel ? FONT_GOLD : FONT_WHITE;
    char *d;
    line[0] = sel ? '>' : ' ';
    line[1] = '1' + s;
    line[2] = 0;
    putAt(b, 3, line, f);
    if (!used[s])
    {
        putAt(b, 8, "- NEW GAME -", f);
        return;
    }
    putAt(b, 7, slotName[s], f);
    d = num(line, slotHearts[s]);
    put(d, slotHearts[s] == 1 ? " HEART" : " HEARTS");
    putAt(b, 15, line, f);
    d = num(line, slotMins[s] / 60);
    *d++ = ':';
    *d++ = '0' + (slotMins[s] % 60) / 10;
    *d++ = '0' + slotMins[s] % 10;
    *d = 0;
    putAt(b, 30 - len(line), line, f);
}

static void drawSlots(u8 cur)
{
    u8 s;
    for (s = 0; s < 3; s++) drawSlot(s, s == cur);
}

/* a row of choices, the chosen one gold */
static void drawChoices(u8 r, const char *lead, const char *const *opt, u8 n, u8 cur)
{
    u16 *b = rowBuf(r);
    u8 i, w = len(lead), c;
    for (i = 0; i < n; i++) w += len(opt[i]) + 3;
    c = (32 - w) / 2;
    putAt(b, c, lead, FONT_WHITE);
    c += len(lead);
    for (i = 0; i < n; i++)
    {
        if (i == cur) putAt(b, c, ">", FONT_GOLD);
        putAt(b, c + 1, opt[i], i == cur ? FONT_GOLD : FONT_WHITE);
        c += len(opt[i]) + 3;
    }
}

static const char *const slotOpt[3] = {"CONTINUE", "ERASE", "BACK"};
static const char *const yesNo[2] = {"NO", "YES"};

/* pick from a row of choices: the choice, or 255 for B */
static u8 choose(u8 r, const char *lead, const char *const *opt, u8 n, u8 cur)
{
    u16 p;
    drawChoices(r, lead, opt, n, cur);
    while (1)
    {
        tFrame();
        p = presses();
        if (p & KEY_B) return 255;
        if (p & (KEY_A | KEY_START)) return cur;
        if ((p & KEY_LEFT) && cur > 0) cur--;
        else if ((p & KEY_RIGHT) && cur + 1 < n) cur++;
        else continue;
        sfx(SFX_MENU);
        drawChoices(r, lead, opt, n, cur);
    }
}

/* ---- name entry ---- */
#define GRID_N 30 /* A-Z, '-', '.', DEL, OK */
#define G_DEL 28
#define G_OK 29
static char nm[9];
static u8 nmLen;

static void gridRow(u8 gr, u8 cur)
{
    u16 *b = rowBuf(10 + gr * 2);
    u8 i, k, c = 6;
    for (i = 0; i < 10; i++)
    {
        k = gr * 10 + i;
        if (k == G_DEL) put(line, "DEL");
        else if (k == G_OK) put(line, "OK");
        else
        {
            line[0] = k < 26 ? 'A' + k : (k == 26 ? '-' : '.');
            line[1] = 0;
        }
        putAt(b, c, line, k == cur ? FONT_GOLD : FONT_WHITE);
        if (k == cur) putAt(b, c - 1, ">", FONT_GOLD);
        c += k == G_DEL ? 4 : 2;
    }
}

static void nameRow(void)
{
    u8 i;
    for (i = 0; i < 8; i++) line[i] = i < nmLen ? nm[i] : '_';
    line[8] = 0;
    rowText(7, line, FONT_GOLD);
}

/* 1 with heroName set, 0 to go back */
static u8 nameEntry(void)
{
    u8 cur = 0, old, i;
    u16 p;
    fade(0);
    loadScreen(11);
    put(nm, "RORY");
    nmLen = 4;
    rowText(5, "WHAT'S YOUR NAME?", FONT_WHITE);
    nameRow();
    tFrame();
    for (i = 0; i < 3; i++) gridRow(i, cur);
    rowText(18, "A: ADD   B: DELETE", FONT_WHITE);
    rowText(20, "START: DONE", FONT_WHITE);
    tFrame();
    fade(1);
    while (1)
    {
        tFrame();
        p = presses();
        old = cur;
        if (p & KEY_START) break;
        if (p & KEY_B)
        {
            if (!nmLen) return 0;
            nmLen--;
            sfx(SFX_MENU);
            nameRow();
            continue;
        }
        if (p & KEY_A)
        {
            if (cur == G_OK) break;
            if (cur == G_DEL)
            {
                if (nmLen) nmLen--;
            }
            else if (nmLen < 8)
            {
                nm[nmLen++] = cur < 26 ? 'A' + cur : (cur == 26 ? '-' : '.');
                if (nmLen == 8) cur = G_OK; /* full: straight to OK */
            }
            sfx(SFX_MENU);
            nameRow();
        }
        if ((p & KEY_LEFT) && cur % 10) cur--;
        if ((p & KEY_RIGHT) && cur % 10 < 9) cur++;
        if ((p & KEY_UP) && cur >= 10) cur -= 10;
        if ((p & KEY_DOWN) && cur < 20) cur += 10;
        if (cur != old)
        {
            if (!(p & KEY_A)) sfx(SFX_MENU);
            gridRow(old / 10, cur);
            if (cur / 10 != old / 10) gridRow(cur / 10, cur);
        }
    }
    sfx(SFX_CHAL);
    if (!nmLen) put(nm, "RORY"), nmLen = 4;
    /* RORY, not R O R Y shouting: the first letter big, the rest small */
    for (i = 0; i < nmLen; i++)
        heroName[i] = (i && nm[i] >= 'A' && nm[i] <= 'Z' && nm[i - 1] != '-' && nm[i - 1] != '.') ? nm[i] + 32 : nm[i];
    heroName[nmLen] = 0;
    return 1;
}

static void titleScreen(u8 cur)
{
    loadScreen(10);
    readSlots();
    drawSlots(cur);
    rowText(ROW_HINT, "A: CHOOSE A SLOT", FONT_WHITE);
    tFrame();
}

/* the whole front end: 1 to continue a saved game (loaded), 0 for a new game (named, slot chosen) */
u8 runTitle(void)
{
    u8 cur = 0, old, k;
    u16 p;
    lastPad = 0xFFFF;
    padSeen = 0;
    rqN = 0;
    setBrightness(0);
    playSong(SONG_TITLE);
    titleScreen(cur);
    fade(1);
    while (1)
    {
        tFrame();
        p = presses();
        old = cur;
        if ((p & KEY_UP) && cur) cur--;
        if ((p & KEY_DOWN) && cur < 2) cur++;
        if (cur != old)
        {
            sfx(SFX_MENU);
            drawSlot(old, 0);
            drawSlot(cur, 1);
            continue;
        }
        if (!(p & (KEY_A | KEY_START))) continue;
        sfx(SFX_MENU);
        if (!used[cur])
        {
            if (nameEntry())
            {
                saveSlot = cur + 1;
                break;
            }
            fade(0);
            titleScreen(cur);
            fade(1);
            continue;
        }
        k = choose(ROW_HINT, "", slotOpt, 3, 0);
        if (k == 0)
        {
            loadGame(cur + 1);
            fade(0);
            return 1;
        }
        if (k == 1 && choose(ROW_HINT, "ERASE IT?  ", yesNo, 2, 0) == 1)
        {
            eraseSlot(cur + 1);
            sfx(SFX_FAIL);
            readSlots();
            drawSlot(cur, 1);
        }
        rowText(ROW_HINT, "A: CHOOSE A SLOT", FONT_WHITE);
    }
    fade(0);
    return 0;
}

/* the opening (web PANELS), with the hero's name where the \x01 is: shown as a text box */
void openIntro(void)
{
    u8 *s = (u8 *)&sys_txt + TXT_INTRO, *d = saveBuf, k;
    do *d++ = *s; while (*s++); /* the title */
    while (*s)
    {
        if (*s == 1)
            for (k = 0; heroName[k]; k++) *d++ = heroName[k];
        else
            *d++ = *s;
        s++;
    }
    *d = 0;
    openTextAt(saveBuf);
}
