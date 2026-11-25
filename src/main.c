/*---------------------------------------------------------------------------------
    River City Quest for the Super Nintendo
    Stage 1: walk the 24 outdoor neighborhoods of Richmond.
---------------------------------------------------------------------------------*/
#include <snes.h>
#include "maps.h"

/* VRAM layout (word addresses). */
#define VRAM_BG1_CHR 0x0000 /* up to 1024 4bpp tiles */
#define VRAM_BG1_MAP 0x4000 /* 64x64 tilemap */
#define VRAM_OBJ_CHR 0x6000

#define MAP_PX_W (MAP_W * 16)
#define MAP_PX_H (MAP_H * 16)
#define SCREEN_W 256
#define SCREEN_H 224
#define HALF 5 /* player collision box half-size, as in the web game */

extern void mapLoad(u8 i);
void drawHero(void);

u8 *colGrid;  /* 32x22 bytes, 1 = solid, for the current map */
u8 curMap;
s16 px, py;   /* player centre in map pixels */
s16 camX, camY;
u8 dir;       /* 0 down, 1 up, 2 left, 3 right */
u8 step;
u16 frameCount;

/* Called by the generated mapLoad(), during forced blank. */
void mapUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *col)
{
    dmaCopyVram(chr, VRAM_BG1_CHR, chrSize);
    dmaCopyVram(map, VRAM_BG1_MAP, 64 * 64 * 2);
    dmaCopyCGram(pal, 0, 8 * 16 * 2);
    colGrid = col;
}

u8 solidAt(s16 x, s16 y)
{
    s16 tx, ty;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H)
        return 0; /* off-map: handled as a map exit */
    tx = x >> 4;
    ty = y >> 4;
    return colGrid[ty * MAP_W + tx];
}

u8 blocked(s16 x, s16 y)
{
    if (solidAt(x - HALF, y - HALF)) return 1;
    if (solidAt(x + HALF, y - HALF)) return 1;
    if (solidAt(x - HALF, y + HALF)) return 1;
    if (solidAt(x + HALF, y + HALF)) return 1;
    return 0;
}

/* Find an open cell nearest (cx,cy), so arrivals never land inside a wall. */
void settle(void)
{
    s16 r, dx, dy;
    if (!blocked(px, py)) return;
    for (r = 16; r < 256; r += 16)
        for (dy = -r; dy <= r; dy += 16)
            for (dx = -r; dx <= r; dx += 16)
                if (!blocked(px + dx, py + dy))
                {
                    px += dx;
                    py += dy;
                    return;
                }
}

void updateCamera(void)
{
    camX = px - SCREEN_W / 2;
    camY = py - SCREEN_H / 2;
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > MAP_PX_W - SCREEN_W) camX = MAP_PX_W - SCREEN_W;
    if (camY > MAP_PX_H - SCREEN_H) camY = MAP_PX_H - SCREEN_H;
    bgSetScroll(0, camX, camY);
}

void enterMap(u8 i)
{
    u8 b;
    for (b = 14; b > 0; b--)
    {
        setBrightness(b);
        WaitForVBlank();
    }
    setBrightness(0); /* forced blank: safe to DMA into VRAM */
    curMap = i;
    mapLoad(i);
    settle();
    updateCamera();
    drawHero();
    WaitForVBlank();
    for (b = 1; b <= 15; b++)
    {
        setBrightness(b);
        WaitForVBlank();
    }
}

void drawHero(void)
{
    u16 tile;
    u8 flip = 0;
    u8 f = (step >> 3) & 1;
    if (dir == 0) tile = 0 + f * 2;
    else if (dir == 1) tile = 4 + f * 2;
    else
    {
        tile = 8 + f * 2;
        if (dir == 2) flip = 1; /* the side frames face right */
    }
    oamSet(0, px - camX - 8, py - camY - 10, 3, flip, 0, tile, 0);
}

/* Walking off an edge moves to the neighboring neighborhood, if there is one. */
void checkExits(void)
{
    u8 n;
    if (px < HALF)
    {
        n = nbW[curMap];
        if (n != 255) { px = MAP_PX_W - HALF - 2; enterMap(n); }
        else px = HALF;
    }
    else if (px > MAP_PX_W - HALF - 1)
    {
        n = nbE[curMap];
        if (n != 255) { px = HALF + 2; enterMap(n); }
        else px = MAP_PX_W - HALF - 1;
    }
    if (py < HALF)
    {
        n = nbN[curMap];
        if (n != 255) { py = MAP_PX_H - HALF - 2; enterMap(n); }
        else py = HALF;
    }
    else if (py > MAP_PX_H - HALF - 1)
    {
        n = nbS[curMap];
        if (n != 255) { py = HALF + 2; enterMap(n); }
        else py = MAP_PX_H - HALF - 1;
    }
}

int main(void)
{
    u16 pad;
    s16 dx, dy, spd;

    setMode(BG_MODE1, 0);
    bgSetGfxPtr(0, VRAM_BG1_CHR);
    bgSetMapPtr(0, VRAM_BG1_MAP, SC_64x64);
    bgSetDisable(1);
    bgSetDisable(2);

    oamInitGfxSet(&hero_chr, (&hero_chrend - &hero_chr), &hero_pal, (&hero_palend - &hero_pal), 0, VRAM_OBJ_CHR, OBJ_SIZE16_L32);
    oamSet(0, 0, 0, 3, 0, 0, 0, 0);
    oamSetEx(0, OBJ_SMALL, OBJ_SHOW);

    px = MAP_PX_W / 2;
    py = MAP_PX_H / 2;
    dir = 0;
    setBrightness(0); /* forced blank while loading */
    curMap = MAP_START;
    mapLoad(curMap);
    settle();
    updateCamera();
    drawHero();
    WaitForVBlank();
    for (spd = 1; spd <= 15; spd++)
    {
        setBrightness(spd);
        WaitForVBlank();
    }

    while (1)
    {
        frameCount++;
        pad = padsCurrent(0);
        dx = 0;
        dy = 0;
        /* 1.5 px per frame, like the web game's walk. */
        spd = (frameCount & 1) ? 2 : 1;
        if (pad & KEY_UP) { dy = -spd; dir = 1; }
        else if (pad & KEY_DOWN) { dy = spd; dir = 0; }
        if (pad & KEY_LEFT) { dx = -spd; dir = 2; }
        else if (pad & KEY_RIGHT) { dx = spd; dir = 3; }

        if (dx || dy)
        {
            step++;
            if (dx && !blocked(px + dx, py)) px += dx;
            if (dy && !blocked(px, py + dy)) py += dy;
            checkExits();
            updateCamera();
        }
        else
            step = 0;

        drawHero();
        WaitForVBlank();
    }
    return 0;
}
