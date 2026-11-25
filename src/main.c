/*---------------------------------------------------------------------------------
    River City Quest for the Super Nintendo
    Stage 1: walk the 24 outdoor neighborhoods of Richmond.
    Stage 2: read signs and landmarks, talk to the people standing around town.
    Stage 3: fighting lives in combat.c.
---------------------------------------------------------------------------------*/
#include "game.h"

/* VRAM layout (word addresses). */
#define VRAM_BG1_CHR 0x0000 /* up to 1024 4bpp tiles */
#define VRAM_BG1_MAP 0x4000 /* 64x64 tilemap */
#define VRAM_BG3_CHR 0x5000 /* 2bpp font and box frame */
#define VRAM_BG3_MAP 0x5800 /* 32x32 text layer */
#define VRAM_OBJ_CHR 0x6000 /* hero at tiles 0-31 */
#define VRAM_OBJ_NPC (VRAM_OBJ_CHR + 32 * 16) /* NPCs from tile 32 */


#define PRIO 0x2000 /* tilemap priority bit: the box draws over sprites */
#define BOX_ROWS 8
#define FILL (FONT_WHITE)  /* a space: box fill colour */

extern void rcqLoadMap(u8 i);
void drawHero(void);

u8 *colGrid;  /* 32x22 bytes, 1 = solid, for the current map */
u8 *wld;      /* reads, NPCs and texts for the current map */
u8 curMap;
s16 px, py;   /* player centre in map pixels */
s16 camX, camY;
u8 heroDir;   /* 0 down, 1 up, 2 left, 3 right */
u8 step;
u16 frameCount;

u8 npcCount;
s16 npcX[MAX_NPCS], npcY[MAX_NPCS];
u8 npcDir[MAX_NPCS], npcTalk[MAX_NPCS];

/* ---------- the text box ---------- */
#define BOX_CLOSED 0
#define BOX_TYPING 1
#define BOX_PAGE 2 /* page full, waiting for A */
#define BOX_END 3  /* text done, waiting for A */

u16 boxMap[BOX_ROWS * 32];
u8 boxState, boxTop, boxDirty;
u8 boxLine, boxCol;
u8 *txt;

void boxPut(u8 row, u8 col, u16 tile)
{
    boxMap[row * 32 + col] = tile | PRIO;
    boxDirty = 1;
}

void boxFillLines(void)
{
    u8 r, c;
    for (r = 2; r <= 6; r++)
        for (c = 2; c <= 29; c++)
            boxMap[r * 32 + c] = FILL | PRIO;
    boxDirty = 1;
}

void boxDrawFrame(void)
{
    u8 r, c;
    boxPut(0, 1, FRAME_TL);
    boxPut(0, 30, FRAME_TR);
    boxPut(7, 1, FRAME_BL);
    boxPut(7, 30, FRAME_BR);
    for (c = 2; c <= 29; c++)
    {
        boxPut(0, c, FRAME_T);
        boxPut(7, c, FRAME_B);
    }
    for (r = 1; r <= 6; r++)
    {
        boxPut(r, 1, FRAME_L);
        boxPut(r, 30, FRAME_R);
        for (c = 2; c <= 29; c++)
            boxPut(r, c, FILL);
    }
}

/* Open a text entry (title NUL, then pages): gold title, then typed-out pages. */
void openTextAt(u8 *p)
{
    u8 c;
    txt = p;
    /* Keep the hero visible: box at the top when they stand low on screen. */
    boxTop = (py - camY > 120) ? 0 : 20;
    boxDrawFrame();
    c = 2;
    while (*txt)
    {
        if (c <= 29)
            boxPut(1, c, FONT_GOLD + *txt - 32);
        c++;
        txt++;
    }
    txt++;
    boxLine = 0;
    boxCol = 0;
    boxState = BOX_TYPING;
}

/* Open text entry i of the current map. */
void openText(u8 i)
{
    u16 ofs;
    ofs = wld[WLD_TOFS + i * 2] | (wld[WLD_TOFS + i * 2 + 1] << 8);
    openTextAt(wld + ofs);
}

void closeText(void)
{
    u16 i;
    for (i = 0; i < BOX_ROWS * 32; i++)
        boxMap[i] = 0;
    boxDirty = 1;
    boxState = BOX_CLOSED;
}

/* Reveal up to n characters, stopping at the end of a page or of the text. */
void typeChars(u8 n)
{
    u8 c;
    while (n)
    {
        c = *txt;
        if (c == 0)
        {
            boxState = BOX_END;
            return;
        }
        txt++;
        if (c == 0x0A)
        {
            boxLine++;
            boxCol = 0;
            continue;
        }
        if (c == 0x0C)
        {
            boxState = BOX_PAGE;
            return;
        }
        boxPut(2 + boxLine, 2 + boxCol, FONT_WHITE + c - 32);
        boxCol++;
        n--;
    }
}

void updateText(u16 pressed)
{
    if (boxState == BOX_TYPING)
    {
        typeChars((pressed & KEY_A) ? 255 : 1); /* A finishes the page at once */
        if (boxState == BOX_TYPING && *txt == 0)
            boxState = BOX_END;
    }
    else
    {
        /* blinking "more" marker in the corner */
        boxPut(6, 29, (frameCount & 16) ? FRAME_MORE : FILL);
        if (pressed & (KEY_A | KEY_B))
        {
            if (boxState == BOX_PAGE)
            {
                boxFillLines();
                boxLine = 0;
                boxCol = 0;
                boxState = BOX_TYPING;
            }
            else
                closeText();
        }
    }
}

/* Call right after WaitForVBlank: copy the box rows to VRAM. */
void flushText(void)
{
    if (!boxDirty)
        return;
    dmaCopyVram((u8 *)boxMap, VRAM_BG3_MAP + boxTop * 32, BOX_ROWS * 32 * 2);
    boxDirty = 0;
}

/* ---------- maps ---------- */

/* Called by the generated rcqLoadMap(), during forced blank. */
void rcqUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *col, u8 *w, u8 *och, u8 *opl, u8 *ene)
{
    u8 k;
    u8 *rec;
    dmaCopyVram(chr, VRAM_BG1_CHR, chrSize);
    dmaCopyVram(map, VRAM_BG1_MAP, 64 * 64 * 2);
    dmaCopyCGram(pal, 0, 8 * 16 * 2);
    dmaCopyVram(och, VRAM_OBJ_NPC, 16 * 6 * 32);
    dmaCopyCGram(opl, 128 + 16, MAX_NPCS * 16 * 2);
    colGrid = col;
    wld = w;
    npcCount = wld[WLD_NNPC];
    for (k = 0; k < npcCount; k++)
    {
        rec = wld + WLD_NPCS + k * 6;
        npcX[k] = rec[0] * 16 + 8;
        npcY[k] = rec[1] * 16 + 8;
        npcDir[k] = 0;
        npcTalk[k] = 0;
    }
    combatLoad(ene);
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
    u8 k;
    if (solidAt(x - HALF, y - HALF)) return 1;
    if (solidAt(x + HALF, y - HALF)) return 1;
    if (solidAt(x - HALF, y + HALF)) return 1;
    if (solidAt(x + HALF, y + HALF)) return 1;
    for (k = 0; k < npcCount; k++)
        if (ABS(x - npcX[k]) < 12 && ABS(y - npcY[k]) < 12)
            return 1;
    return 0;
}

/* Find an open spot near the player, so arrivals never land inside a wall. */
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

void drawNpcs(void)
{
    u8 k, f, idx, flip;
    u16 id;
    s16 sx, sy;
    for (k = 0; k < MAX_NPCS; k++)
    {
        id = (OAM_NPCS + k) * 4;
        if (k >= npcCount)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        sx = npcX[k] - camX - 8;
        sy = npcY[k] - camY - 10;
        if (sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            oamSetEx(id, OBJ_LARGE, OBJ_HIDE);
            continue;
        }
        f = npcDir[k] == 0 ? 0 : npcDir[k] == 1 ? 1 : 2;
        flip = npcDir[k] == 2;
        idx = k * 3 + f;
        oamSetEx(id, OBJ_LARGE, OBJ_SHOW);
        oamSet(id, sx & 0x1FF, sy & 0xFF, 2, flip, 0, 32 + (idx >> 3) * 32 + (idx & 7) * 2, k + 1);
    }
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
    rcqLoadMap(i);
    settle();
    updateCamera();
    drawHero();
    drawNpcs();
    combatDraw();
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
    if (heroDir == 0) tile = 0 + f * 2;
    else if (heroDir == 1) tile = 4 + f * 2;
    else
    {
        tile = 8 + f * 2;
        if (heroDir == 2) flip = 1; /* the side frames face right */
    }
    if (inv & 4)
    {
        oamSetEx(OAM_HERO * 4, OBJ_LARGE, OBJ_HIDE); /* blink while invincible */
        return;
    }
    oamSetEx(OAM_HERO * 4, OBJ_LARGE, OBJ_SHOW);
    oamSet(OAM_HERO * 4, (px - camX - 8) & 0x1FF, (py - camY - 10) & 0xFF, 3, flip, 0, tile, 0);
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

/* A button: talk to whoever is in front, or read what is there. Returns 1 if it did. */
u8 interact(void)
{
    s16 fx = px, fy = py;
    u8 k, t;
    u8 *rec;
    if (heroDir == 0) fy += 14;
    else if (heroDir == 1) fy -= 14;
    else if (heroDir == 2) fx -= 14;
    else fx += 14;
    for (k = 0; k < npcCount; k++)
        if (ABS(fx - npcX[k]) < 13 && ABS(fy - npcY[k]) < 13)
        {
            rec = wld + WLD_NPCS + k * 6;
            npcDir[k] = heroDir == 0 ? 1 : heroDir == 1 ? 0 : heroDir == 2 ? 3 : 2;
            openText(rec[2] + npcTalk[k]);
            npcTalk[k]++;
            if (npcTalk[k] >= rec[3]) npcTalk[k] = 0;
            return 1;
        }
    if (fx < 0 || fy < 0 || fx >= MAP_PX_W || fy >= MAP_PX_H)
        return 0;
    t = wld[WLD_GRID + (fy >> 4) * MAP_W + (fx >> 4)];
    if (!t)
        return 0;
    openText(t - 1);
    return 1;
}

/* After "The fog takes you...": wake by the fountain in Capitol Square (web: 7,8 facing left). */
void respawn(void)
{
    dying = 0;
    hp = hpMax;
    inv = 60;
    px = 7 * 16 + 8;
    py = 8 * 16 + 8;
    heroDir = 2;
    enterMap(MAP_CAPITOL);
}

int main(void)
{
    u16 pad, pressed, i;
    s16 dx, dy, spd;

    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetGfxPtr(0, VRAM_BG1_CHR);
    bgSetMapPtr(0, VRAM_BG1_MAP, SC_64x64);
    bgSetGfxPtr(2, VRAM_BG3_CHR);
    bgSetMapPtr(2, VRAM_BG3_MAP, SC_32x32);
    bgSetDisable(1);

    /* 8x8 small sprites for the HUD and pickups, 16x16 large for everyone else */
    oamInitGfxSet(&hero_chr, (&hero_chrend - &hero_chr), &hero_pal, (&hero_palend - &hero_pal), 0, VRAM_OBJ_CHR, OBJ_SIZE8_L16);
    for (i = 0; i < 128; i++)
        oamSetEx(i * 4, OBJ_SMALL, OBJ_HIDE);

    px = MAP_PX_W / 2;
    py = MAP_PX_H / 2;
    heroDir = 0;
    setBrightness(0); /* forced blank while loading */

    /* Enemies, the paddle and the HUD: OBJ tiles 128-255, sprite palettes 5-7. */
    dmaCopyVram(&combat_chr, VRAM_OBJ_CHR + 128 * 16, (&combat_chrend - &combat_chr));
    dmaCopyCGram(&combat_pal, 128 + 5 * 16, 3 * 16 * 2);

    /* Font, and an empty text layer. */
    dmaCopyVram(&font_chr, VRAM_BG3_CHR, (&font_chrend - &font_chr));
    for (i = 0; i < BOX_ROWS * 32; i++)
        boxMap[i] = 0;
    for (i = 0; i < 4; i++)
        dmaCopyVram((u8 *)boxMap, VRAM_BG3_MAP + i * BOX_ROWS * 32, BOX_ROWS * 32 * 2);
    bgSetScroll(2, 0, 0);

    curMap = MAP_START;
    rcqLoadMap(curMap);
    settle();
    updateCamera();
    drawHero();
    drawNpcs();
    combatDraw();
    hudDraw();
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
        pressed = padsDown(0);

        if (boxState != BOX_CLOSED)
        {
            updateText(pressed);
            step = 0;
            if (boxState == BOX_CLOSED && dying)
            {
                WaitForVBlank();
                flushText();
                respawn();
            }
        }
        else if ((pressed & KEY_A) && !heroBusy() && !charging && interact())
        {
            step = 0;
        }
        else
        {
            combatUpdate(pad, pressed);
            dx = 0;
            dy = 0;
            /* 1.5 px per frame like the web game's walk; slower while charging a spin */
            spd = (frameCount & 1) ? 2 : 1;
            if (charging) spd = (frameCount % 5) ? 1 : 0;
            if (!heroBusy() && !dying)
            {
                if (pad & KEY_UP) { dy = -spd; if (!charging) heroDir = 1; }
                else if (pad & KEY_DOWN) { dy = spd; if (!charging) heroDir = 0; }
                if (pad & KEY_LEFT) { dx = -spd; if (!charging) heroDir = 2; }
                else if (pad & KEY_RIGHT) { dx = spd; if (!charging) heroDir = 3; }
            }

            if (dx || dy)
            {
                step++;
                if (dx && !blocked(px + dx, py)) px += dx;
                if (dy && !blocked(px, py + dy)) py += dy;
                checkExits();
            }
            else if (!charging)
                step = 0;
            updateCamera();
        }

        drawHero();
        drawNpcs();
        combatDraw();
        hudDraw();
        WaitForVBlank();
        flushText();
    }
    return 0;
}
