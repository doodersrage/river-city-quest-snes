/* The ending (web startRelight, the win screen and CREDITS): after the Richmond Vampire's
   last words, five neighbourhoods with their names, then pages of credits on black, then
   back to Church Hill outside the tunnel. */
#include "game.h"

extern u8 boxDirty, retMap;
extern u16 boxMap[];
void rcqLoadMap(u8 i);
void settle(void);
void updateCamera(void);
void flushText(void);

static void fade(u8 in)
{
    u8 b;
    for (b = 1; b <= 15; b++)
    {
        setBrightness(in ? b : 15 - b);
        WaitForVBlank();
    }
}

/* wait up to n frames, or until A or START */
static void hold(u16 n)
{
    u16 t;
    for (t = 0; t < n; t++)
    {
        WaitForVBlank();
        if (t > 30 && (padsDown(0) & (KEY_A | KEY_START))) return;
    }
}

static void hideSprites(void)
{
    u8 i;
    for (i = 0; i < 128; i++) sprHide(i);
}

static u16 row[32];

/* one line of text centred on BG3 row r (forced blank only) */
static void bg3Line(u8 r, const char *s, u16 font)
{
    u8 n = 0, c;
    for (c = 0; c < 32; c++) row[c] = 0;
    while (s[n]) n++;
    for (c = 0; c < n && c < 32; c++) row[(32 - n) / 2 + c] = (font + s[c] - 32) | 0x2000;
    dmaCopyVram((u8 *)row, VRAM_BG3_MAP + r * 32, 64);
}

static void bg3Clear(void)
{
    u8 r;
    for (r = 0; r < 32; r++) bg3Line(r, "", FONT_WHITE);
}

/* "Starring RORY" in the credits: whoever you named */
static char star[24];
static const char *starring(const char *s)
{
    const char *w = "Starring ";
    u8 k = 0, j;
    while (w[k] && s[k] == w[k]) k++;
    if (w[k]) return s;
    for (j = 0; j < k; j++) star[j] = w[j];
    for (j = 0; heroName[j]; j++) star[k + j] = heroName[j];
    star[k + j] = 0;
    return star;
}

void runEnding(void)
{
    u8 k, n, first;
    u16 i;
    fade(0);
    closeText();
    hideSprites();
    playSong(SONG_WIN);
    /* the lamps come back on, one neighbourhood after another */
    for (k = 0; k < RELIGHT_N; k++)
    {
        if (k) fade(0);
        setBrightness(0);
        curMap = relightMap[k];
        rcqLoadMap(curMap);
        px = MAP_PX_W / 2;
        py = MAP_PX_H / 2;
        updateCamera();
        dayClock = 15 * 1500; /* 9 at night: the lamps come back on */
        lightFrame();
        hideSprites();
        bg3Clear();
        bg3Line(1, "THE LAMPS COME BACK ON", FONT_GOLD);
        bg3Line(26, relightName[k], FONT_WHITE);
        WaitForVBlank();
        fade(1);
        hold(200);
    }
    /* credits on black */
    fade(0);
    setBrightness(0);
    bgSetDisable(0);
    hideSprites();
    dayClock = 0; /* the credits run through to morning */
    lightFrame();
    i = 0;
    while (i < CREDIT_LINES)
    {
        setBrightness(0);
        bg3Clear();
        n = 0;
        while (i + n < CREDIT_LINES && creditLine[i + n][0] != '\f') n++;
        first = 14 - n;
        for (k = 0; k < n; k++)
            bg3Line(first + k * 2, starring(creditLine[i + k]), k == 0 && i == 0 ? FONT_GOLD : FONT_WHITE);
        WaitForVBlank();
        fade(1);
        hold(240);
        fade(0);
        i += n + 1;
    }
    /* back outside the tunnel mouth on Church Hill */
    setBrightness(0);
    bg3Clear();
    for (i = 0; i < BOX_ROWS_ALL; i++) boxMap[i] = 0;
    boxDirty = 0;
    bgSetEnable(0);
    curMap = MAP_WIN;
    retMap = 255;   /* outside now: no dungeon to come back to */
    dentMap = 255;
    rcqLoadMap(curMap);
    px = WIN_X;
    py = WIN_Y;
    heroDir = 0;
    hp = hpMax;
    settle();
    updateCamera();
    songForMap();
    WaitForVBlank();
    fade(1);
}
