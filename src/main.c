/*---------------------------------------------------------------------------------
    River City Quest for the Super Nintendo
    Stage 1: walk the 24 outdoor neighborhoods of Richmond.
    Stage 2: read signs and landmarks, talk to the people standing around town.
    Stage 3: fighting lives in combat.c.
---------------------------------------------------------------------------------*/
#include "game.h"

/* VRAM layout (word addresses, see game.h): BG1 tiles 0x0000, BG1 map 0x4000 (64x64),
   BG3 font 0x5000, BG3 map 0x5800, sprites 0x6000 (hero 0-31), boss art 0x7000 (tiles 256+). */
#define VRAM_OBJ_NPC (VRAM_OBJ_CHR + 32 * 16) /* NPCs from tile 32 */
#define NPC_BYTES (9 * 256)  /* one person's frames */
u8 *npcChr;              /* the map's people: 9 frames each, 256 bytes a frame */
u8 npcShown[MAX_NPCS];


#define PRIO 0x2000 /* tilemap priority bit: the box draws over sprites */
#define BOX_ROWS 8

extern void rcqLoadMap(u8 i);
void drawHero(void);

u8 *colGrid;  /* 32x22 cells (stride 32): CELL_SOLID, CELL_DOOR, CELL_EXIT */
u8 mapW, mapH;
u8 *doors;    /* count, then tx, ty, target map, need */
u8 *chests;   /* count, then tx, ty, id, effect, amount, text, 4 tilemap words */
u8 retMap, retX, retY; /* where an interior's exit leads */
u8 chestOpen[(CHEST_COUNT + 7) / 8];

u8 *bushes;   /* count, then tx, ty, 4 tilemap words of cut ground */
u8 colRam[COL_STRIDE * 22]; /* the collision grid, copied to RAM so cut bushes open up */

/* 2x2-cell tile changes to copy into the BG1 tilemap during the next vblank */
#define MAX_PATCH 16
u16 patchWords[MAX_PATCH][4];
u16 patchAddr[MAX_PATCH];
u8 patchCount;
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

/* ---------- sprites: only touch OAM entries that change ---------- */
u8 oamShown[128]; /* 0 hidden, 1 shown small, 2 shown large */

void sprHide(u8 slot)
{
    if (!oamShown[slot]) return;
    oamSetEx(slot * 4, OBJ_SMALL, OBJ_HIDE);
    oamShown[slot] = 0;
}

void sprPut(u8 slot, u8 large, s16 x, s16 y, u8 prio, u8 flip, u16 tile, u8 pal)
{
    if (oamShown[slot] != large + 1)
    {
        oamSetEx(slot * 4, large ? OBJ_LARGE : OBJ_SMALL, OBJ_SHOW);
        oamShown[slot] = large + 1;
    }
    oamSet(slot * 4, x & 0x1FF, y & 0xFF, prio, flip, 0, tile, pal);
}

/* ---------- the text box ---------- */
#define BOX_CLOSED 0
#define BOX_TYPING 1
#define BOX_PAGE 2 /* page full, waiting for A */
#define BOX_END 3  /* text done, waiting for A */
#define BOX_ASK 6  /* a question: pick one of askN answers */
#define BOX_SHOP 4 /* shop list */

u8 shopId, shopCur, shopTop, shopCount, shopReturn;
u8 *shopBase;
u8 shopOwned[2][MAX_SHOP_ITEMS];

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
    sfx(SFX_TALK);
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

void drawShop(void);

void closeText(void)
{
    u16 i;
    if (cardReturn)
    {
        cardReturn = 0;
        drawCards();
        return;
    }
    if (shopReturn)
    {
        shopReturn = 0;
        drawShop();
        return;
    }
    for (i = 0; i < BOX_ROWS * 32; i++)
        boxMap[i] = 0;
    boxDirty = 1;
    boxState = BOX_CLOSED;
}

/* ---------- shops (Carytown Goods, the 17th Street Market) ---------- */

void boxText(u8 row, u8 col, u8 *s, u16 font)
{
    while (*s && col <= 29)
    {
        boxPut(row, col, font + *s - 32);
        s++;
        col++;
    }
}

/* text right-aligned so its last letter is at column `right` */
void boxTextRight(u8 row, u8 right, u8 *s, u16 font)
{
    u8 n = 0;
    while (s[n]) n++;
    boxText(row, right + 1 - n, s, font);
}

/* number right-aligned so its last digit is at column `right` */
void boxNum(u8 row, u8 right, u16 v)
{
    do
    {
        boxPut(row, right, FONT_WHITE + '0' + (v % 10) - 32);
        v /= 10;
        right--;
    } while (v);
}

u8 *shopItem(u8 j)
{
    return shopBase + SHOP_HEAD + j * SHOP_ITEM;
}

/* 0 buyable, 1 sold out, 2 locked (shows its lock text), 3 full */
u8 shopStatus(u8 j)
{
    u8 *it = shopItem(j);
    u8 kind = it[SHOP_NAME + SHOP_LOCK + 1], req = it[SHOP_NAME + SHOP_LOCK + 2], e;
    if (kind == SHOP_LOCKED) return 2;
    if (kind == SHOP_NEEDS)
    {
        if (!hasItem(req)) return 2;
        e = it[SHOP_NAME + SHOP_LOCK + 3];
        if ((e == EFFECT_BALLS && balls >= capBalls) || (e == EFFECT_KEGS && powder >= capPowder) || (e == EFFECT_GINGER && bottleFull)) return 3;
        return 0;
    }
    if (req != 255 && !shopOwned[shopId][req]) return 2;
    if (kind == SHOP_ONCE && shopOwned[shopId][j]) return 1;
    if (it[SHOP_NAME + SHOP_LOCK + 3] == EFFECT_CARD && packSoldOut()) return 1;
    return 0;
}

void drawShop(void)
{
    u8 r, j, st;
    u8 *it;
    boxDrawFrame();
    boxText(1, 2, shopBase, FONT_GOLD);
    for (r = 0; r < 4; r++)
    {
        j = shopTop + r;
        if (j >= shopCount) break;
        it = shopItem(j);
        boxPut(2 + r, 2, j == shopCur ? FONT_GOLD + '>' - 32 : FILL);
        boxText(2 + r, 3, it, FONT_WHITE);
        st = shopStatus(j);
        if (st == 1) boxText(2 + r, 22, (u8 *)"SOLD OUT", FONT_GOLD);
        else if (st == 2) boxTextRight(2 + r, 29, it + SHOP_NAME, FONT_GOLD);
        else if (st == 3) boxTextRight(2 + r, 29, (u8 *)"FULL", FONT_GOLD);
        else boxNum(2 + r, 29, it[SHOP_NAME + SHOP_LOCK]);
    }
    boxText(6, 2, (u8 *)"A: BUY   B: LEAVE", FONT_GOLD);
    boxState = BOX_SHOP;
}

void openShop(u8 id)
{
    shopId = id;
    shopBase = (u8 *)&shops_bin + (id ? SHOP_OFS_MARKET : SHOP_OFS_SHOP);
    shopCount = shopBase[24];
    shopCur = 0;
    shopTop = 0;
    shopReturn = 0;
    boxTop = (py - camY > 120) ? 0 : 20;
    drawShop();
}

void shopBuy(void)
{
    u8 *it = shopItem(shopCur);
    u8 st = shopStatus(shopCur), price = it[SHOP_NAME + SHOP_LOCK];
    u16 mo;
    shopReturn = 1;
    if (st)
    {
        openTextAt((u8 *)&sys_txt + (st == 1 ? TXT_SOLDOUT : st == 3 ? TXT_FULL : TXT_NOTYET));
        return;
    }
    if (coins < price)
    {
        openTextAt((u8 *)&sys_txt + TXT_NOCOINS);
        return;
    }
    coins -= price;
    if (shopOwned[shopId][shopCur] < 255) shopOwned[shopId][shopCur]++;
    if (it[SHOP_NAME + SHOP_LOCK + 3] == EFFECT_HEART && hpMax < 20)
    {
        hpMax += 2;
        hp = hpMax;
    }
    switch (it[SHOP_NAME + SHOP_LOCK + 3])
    {
    case EFFECT_BALLS:
        balls = balls + 10 > capBalls ? capBalls : balls + 10;
        break;
    case EFFECT_KEGS:
        powder = powder + 4 > capPowder ? capPowder : powder + 4;
        break;
    case EFFECT_GINGER:
        bottleFull = 1;
        break;
    case EFFECT_SATCHEL:
        capBalls = 50;
        capPowder = 16;
        break;
    case EFFECT_ROD:
        hasRod = 1;
        if (!eqItem) eqItem = ITEM_ROD;
        break;
    case EFFECT_CARD:
        gotPackCard(); /* says which card */
        return;
    case EFFECT_CAMERA:
        hasCamera = 1;
        if (!eqItem) eqItem = ITEM_CAMERA;
        break;
    }
    mo = it[SHOP_NAME + SHOP_LOCK + 4] | (it[SHOP_NAME + SHOP_LOCK + 5] << 8);
    openTextAt((u8 *)&shops_bin + SHOP_MSGS + mo);
}

void updateShop(u16 pressed)
{
    if (pressed & KEY_UP && shopCur > 0)
    {
        shopCur--;
        sfx(SFX_MENU);
        if (shopCur < shopTop) shopTop = shopCur;
        drawShop();
    }
    else if (pressed & KEY_DOWN && shopCur + 1 < shopCount)
    {
        shopCur++;
        sfx(SFX_MENU);
        if (shopCur >= shopTop + 4) shopTop = shopCur - 3;
        drawShop();
    }
    else if (pressed & KEY_A)
        shopBuy();
    else if (pressed & KEY_B)
        closeText();
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

/* The question at the end of a text: the answers on the lines below the title, > on the chosen one. */
void drawAsk(void)
{
    u8 i;
    boxFillLines();
    for (i = 0; i < askN; i++)
    {
        boxPut(3 + i, 2, i == askCur ? FONT_GOLD + '>' - 32 : FILL);
        boxText(3 + i, 3, (u8 *)askOpt[i], i == askCur ? FONT_GOLD : FONT_WHITE);
    }
    boxState = BOX_ASK;
}

void updateAsk(u16 pressed)
{
    if ((pressed & KEY_UP) && askCur > 0)
    {
        askCur--;
        sfx(SFX_MENU);
        drawAsk();
    }
    else if ((pressed & KEY_DOWN) && askCur + 1 < askN)
    {
        askCur++;
        sfx(SFX_MENU);
        drawAsk();
    }
    else if (pressed & KEY_A)
        askChoose(askCur);
    else if ((pressed & KEY_B) && askKind != ASK_QUIZ)
        askChoose(1); /* not now */
}

void updateText(u16 pressed)
{
    if (boxState == BOX_ASK)
    {
        updateAsk(pressed);
        return;
    }
    if (boxState == 7) /* BOX_FISH: a line in the water */
    {
        fishUpdate(pressed);
        return;
    }
    if (boxState == 8) /* BOX_CARDS */
    {
        updateCards(pressed);
        return;
    }
    if (boxState == BOX_SHOP)
    {
        updateShop(pressed);
        return;
    }
    if (boxState == BOX_MENU)
    {
        updateMenu(pressed);
        return;
    }
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
            else if (askKind)
                drawAsk();
            else
                closeText();
        }
    }
}

/* Call right after WaitForVBlank: copy the box rows to VRAM. */
void flushText(void)
{
    u8 k;
    for (k = 0; k < patchCount; k++)
    {
        dmaCopyVram((u8 *)&patchWords[k][0], patchAddr[k], 4);
        dmaCopyVram((u8 *)&patchWords[k][2], patchAddr[k] + 32, 4);
    }
    patchCount = 0;
    if (!boxDirty)
        return;
    dmaCopyVram((u8 *)boxMap, VRAM_BG3_MAP + boxTop * 32, BOX_ROWS * 32 * 2);
    boxDirty = 0;
}

/* ---------- maps ---------- */

/* Called by the generated rcqLoadMap(), during forced blank. */
u16 cellAddr(u8 cx, u8 cy)
{
    return VRAM_BG1_MAP + ((cy >> 5) * 2 + (cx >> 5)) * 1024 + (cy & 31) * 32 + (cx & 31);
}

u8 *chestRec(u8 k)
{
    return chests + 1 + k * 23;
}

/* Queue a 2x2 tile change for 16px cell (tx, ty); w points at 4 little-endian words.
   now = 1 during forced blank: a full queue is written out at once instead of dropping changes. */
void queuePatch(u8 tx, u8 ty, u8 *w, u8 now)
{
    u8 i;
    if (patchCount >= MAX_PATCH)
    {
        if (!now) return;
        flushText();
    }
    for (i = 0; i < 4; i++)
        patchWords[patchCount][i] = w[i * 2] | (w[i * 2 + 1] << 8);
    patchAddr[patchCount] = cellAddr(tx * 2, ty * 2); /* the row below is +32 words */
    patchCount++;
}

/* Queue (or, during forced blank, write now) the opened-chest tiles for chest k. */
void chestTiles(u8 k, u8 now)
{
    u8 *r = chestRec(k);
    queuePatch(r[0], r[1], r + 15, now);
    if (now) flushText();
}

/* The paddle reached (x, y): cut a bush there (web cutAt), maybe dropping a coin or heart. */
void cutAt(s16 x, s16 y)
{
    u8 tx, ty, k, r;
    u8 *b;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H) return;
    tx = x >> 4;
    ty = y >> 4;
    if (colRam[ty * COL_STRIDE + tx] != CELL_BUSH) return;
    colRam[ty * COL_STRIDE + tx] = 0;
    for (k = 0; k < bushes[0]; k++)
    {
        b = bushes + 1 + k * 10;
        if (b[0] == tx && b[1] == ty)
        {
            queuePatch(tx, ty, b + 2, 0);
            break;
        }
    }
    r = rnd() % 10;
    if (r < 2) dropKind(tx * 16 + 8, ty * 16 + 8, 1);
    else if (r < 3) dropKind(tx * 16 + 8, ty * 16 + 8, 2);
}

void rcqUpload(u8 *chr, u16 chrSize, u8 *map, u8 *pal, u8 *col, u8 *w, u8 *och, u8 *opl, u8 *ene, u8 *xtr)
{
    u8 k, gid;
    u16 n;
    u8 *rec;
    dmaCopyVram(chr, VRAM_BG1_CHR, chrSize);
    dmaCopyVram(map, VRAM_BG1_MAP, 64 * 64 * 2);
    dmaCopyCGram(pal, 0, 8 * 16 * 2);
    /* stage 12: people are 16x32 and streamed like the hero; here, while the screen is off, each one's first frame */
    npcChr = och;
    for (k = 0; k < MAX_NPCS; k++)
    {
        dmaCopyVram(och + k * NPC_BYTES, VRAM_OBJ_NPC + k * 4 * 16, 128);
        dmaCopyVram(och + k * NPC_BYTES + 128, VRAM_OBJ_NPC + (16 + k * 4) * 16, 128);
        npcShown[k] = 0;
    }
    npcPend = 0;
    dmaCopyCGram(opl, 128 + 16, MAX_NPCS * 16 * 2);
    colGrid = col;
    wld = w;
    mapW = xtr[0];
    if (mapW != 32) riding = 0; /* the bike stays outside */
    mapH = xtr[1];
    doors = xtr + 2;
    chests = doors + 1 + doors[0] * 4;
    bushes = chests + 1 + chests[0] * 23;
    rec = bushes + 1 + bushes[0] * 10;
    roomArea = rec[0];
    roomSolve = rec[1];
    roomBoss = rec[2];
    roomEntry = rec[3] & 1;
    roomReward = rec[3] >> 4;
    dynPtr = rec + 4;
    rec = dynPtr + 1;
    for (k = 0; k < dynPtr[0]; k++)
        rec += 4 + rec[2] * 10;
    stairsPtr = rec;
    brazPtr = stairsPtr + 1 + stairsPtr[0] * 10;
    pickPtr = brazPtr + 1 + brazPtr[0] * 10;
    blkPtr = pickPtr + 1 + pickPtr[0] * 6;
    for (n = 0; n < COL_STRIDE * 22; n++)
        colRam[n] = col[n];
    colGrid = colRam;

    npcCount = wld[WLD_NNPC];
    for (k = 0; k < npcCount; k++)
    {
        rec = wld + WLD_NPCS + k * NPC_REC;
        npcX[k] = rec[0] * 16 + 8;
        npcY[k] = rec[1] * 16 + 8;
        npcDir[k] = 0;
        npcTalk[k] = 0;
    }
    combatLoad(ene);
    dungeonLoad();
    lifeLoad();
}

u8 solidAt(s16 x, s16 y)
{
    s16 tx, ty;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H)
        return 0; /* off-map: handled as a map exit */
    tx = x >> 4;
    ty = y >> 4;
    tx = colGrid[ty * COL_STRIDE + tx];
    if (tx == CELL_RIVER) return !hasKayak; /* the kayak paddles the James */
    return tx == CELL_SOLID || tx == CELL_COUNTER || tx == CELL_BUSH || tx == CELL_POST || tx == CELL_LOW || tx == CELL_WATER || tx == CELL_LOCK || tx == CELL_BRAZIER || tx == CELL_SPROUT;
}

u8 blocked(s16 x, s16 y)
{
    u8 k;
    if (solidAt(x - HALF, y - HALF)) return 1;
    if (solidAt(x + HALF, y - HALF)) return 1;
    if (solidAt(x - HALF, y + HALF)) return 1;
    if (solidAt(x + HALF, y + HALF)) return 1;
    /* push blocks */
    if (blockAtCell((x - HALF) >> 4, (y - HALF) >> 4) != 255 || blockAtCell((x + HALF) >> 4, (y - HALF) >> 4) != 255 ||
        blockAtCell((x - HALF) >> 4, (y + HALF) >> 4) != 255 || blockAtCell((x + HALF) >> 4, (y + HALF) >> 4) != 255)
        return 1;
    for (k = 0; k < npcCount; k++)
        if (ABS(x - npcX[k]) < 12 && ABS(y - npcY[k]) < 12)
            return 1;
    /* cars and the bus (never trap you if one is already on top of you) */
    if (carHit(x, y) && !carHit(px, py)) return 1;
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
    /* rooms smaller than the screen sit in the middle (the area around them is blank tile 0) */
    if (MAP_PX_W < SCREEN_W) camX = -(SCREEN_W - MAP_PX_W) / 2;
    if (MAP_PX_H < SCREEN_H) camY = -(SCREEN_H - MAP_PX_H) / 2;
    bgSetScroll(0, camX, camY);
}

static const u8 walkFrame[4] = {0, 1, 0, 2}; /* stand, step, stand, the other step */

/* Stage 12: each person on the map has a 16x32 slot in VRAM (OBJ tiles 32+4k and 48+4k) holding the frame on
   show. A changed frame waits its turn: one person's frame is copied a VBlank, by npcVBlank. */
u8 npcPend;
u8 *npcSrc;
u16 npcDst;

void npcVBlank(void)
{
    if (!npcPend) return;
    dmaCopyVram(npcSrc, npcDst, 128);
    dmaCopyVram(npcSrc + 128, npcDst + 16 * 16, 128);
    npcPend = 0;
}

extern s8 npcVx[], npcVy[];

void drawNpcs(void)
{
    u8 k, f, flip;
    s16 sx, sy;
    for (k = 0; k < MAX_NPCS; k++)
    {
        if (k >= npcCount)
        {
            sprHide(OAM_NPCS + k);
            sprHide(OAM_NPCTOP + k);
            continue;
        }
        sx = npcX[k] - camX - 8;
        sy = npcY[k] - camY - 10;
        if (sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H + 16)
        {
            sprHide(OAM_NPCS + k);
            sprHide(OAM_NPCTOP + k);
            continue;
        }
        f = npcDir[k] == 0 ? 0 : npcDir[k] == 1 ? 3 : 6;
        if ((npcVx[k] || npcVy[k]) && !boxState) f += walkFrame[(frameCount >> 3) & 3];
        if (f != npcShown[k] && !npcPend)
        {
            npcShown[k] = f;
            npcSrc = npcChr + k * NPC_BYTES + (u16)f * 256;
            npcDst = VRAM_OBJ_NPC + k * 4 * 16;
            npcPend = 1;
        }
        flip = npcDir[k] == 2;
        sprPut(OAM_NPCTOP + k, 1, sx, sy - 16, 2, flip, 32 + k * 4, k + 1);
        sprPut(OAM_NPCS + k, 1, sx, sy, 2, flip, 32 + k * 4 + 2, k + 1);
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
    kt = 0;           /* a hit taken at the edge doesn't carry you back across it */
    lastCell = 0xFFFF;
    dash = 0;
    curMap = i;
    rcqLoadMap(i);
    settle();
    updateCamera();
    drawHero();
    drawNpcs();
    lifeDraw();
    combatDraw();
    saveGame(); /* every arrival is a save, like the web game's autosave */
    WaitForVBlank();
    songForMap();
    for (b = 1; b <= 15; b++)
    {
        setBrightness(b);
        WaitForVBlank();
    }
    dungeonEntered();
}

/* Stage 12: the hero is 16x32, two 16x16 sprites (the head in OAM_HEROTOP). Only the frame on show is in VRAM,
   OBJ tiles 0-3 and 16-19: when it changes, heroVBlank copies the new one from ROM at the start of the next VBlank. */
u8 heroShown, heroPend;
u8 *heroSrc;

void heroFrame(u8 f)
{
    if (f == heroShown) return;
    heroShown = f;
    heroSrc = (u8 *)&hero_chr + (u16)f * 256;
    heroPend = 1;
}

/* from rcqVBlank (only scalars and pointers here: the data bank is not ours) */
void heroVBlank(void)
{
    if (!heroPend) return;
    dmaCopyVram(heroSrc, VRAM_OBJ_CHR, 128);
    dmaCopyVram(heroSrc + 128, VRAM_OBJ_CHR + 16 * 16, 128);
    heroPend = 0;
}

void drawHero(void)
{
    u8 flip = heroDir == 2; /* the side frames face right */
    heroFrame((heroDir == 0 ? 0 : heroDir == 1 ? 3 : 6) + walkFrame[(step >> 3) & 3]);
    if (inv & 4)
    {
        sprHide(OAM_HERO); /* blink while invincible */
        sprHide(OAM_HEROTOP);
        return;
    }
    sprPut(OAM_HEROTOP, 1, px - camX - 8, py - camY - 26, 3, flip, 0, 0);
    sprPut(OAM_HERO, 1, px - camX - 8, py - camY - 10, 3, flip, 2, 0);
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
    /* across a shop counter, reach the person behind it (web: front(30)) */
    if (fx >= 0 && fy >= 0 && fx < MAP_PX_W && fy < MAP_PX_H && colGrid[(fy >> 4) * COL_STRIDE + (fx >> 4)] == CELL_COUNTER)
    {
        fx = px + (fx - px) * 30 / 14;
        fy = py + (fy - py) * 30 / 14;
    }
    for (k = 0; k < npcCount; k++)
        if (ABS(fx - npcX[k]) < 13 && ABS(fy - npcY[k]) < 13)
        {
            rec = wld + WLD_NPCS + k * NPC_REC;
            npcDir[k] = heroDir == 0 ? 1 : heroDir == 1 ? 0 : heroDir == 2 ? 3 : 2;
            if (rec[5])
            {
                openShop(rec[5] - 1);
                return 1;
            }
            /* the bateau: the river historian, once you bring the oar, the pole and the tiller */
            if (rec[8] == 7)
            {
                if (bateauDone)
                {
                    openText(rec[9] + 1);
                    return 1;
                }
                if (parts >= 3)
                {
                    bateauDone = 1;
                    hasLure = 1;
                    coins = coins + 50 > 999 ? 999 : coins + 50;
                    if (hpMax < 20) hpMax += 2; /* the heart charm */
                    hp = hpMax;
                    sfx(SFX_JING);
                    openText(rec[9]);
                    return 1;
                }
            }
            /* the frog count: the naturalist in Byrd Park, with five frogs */
            else if (rec[8] == 9 && (frogsDone || frogs >= 5))
            {
                if (frogsDone)
                {
                    openText(rec[9] + 1);
                    return 1;
                }
                frogsDone = 1;
                coins = coins + 40 > 999 ? 999 : coins + 40;
                cards |= 1 << CARD_MAYMONT;
                sfx(SFX_JING);
                openText(rec[9]);
                return 1;
            }
            /* the trading quest: melon -> sheet music -> river map -> kayak */
            else if (rec[8] == 5)
            {
                if (hasBib && !hasShoes)
                {
                    openText(rec[9]);
                    gotItem(ITEM_SHOES);
                    return 1;
                }
            }
            else if (rec[8] != 255 && ((rec[8] == 0 && !tradeItem && !hasKayak && !melonTaken) || (rec[8] && tradeItem == rec[8])))
            {
                openText(rec[9]);
                if (rec[10] == 4)
                {
                    hasKayak = 1;
                    tradeItem = 0;
                }
                else tradeItem = rec[10];
                if (rec[10] == 1) melonTaken = 1;
                return 1;
            }
            /* the Brewer tops up your hearts, and refills an empty bottle; the Ballplayer refills your balls */
            if (rec[7] == ITEM_BOTTLE) hp = hpMax;
            if (rec[11] != 255 && rec[7] && hasItem(rec[7]))
            {
                if (rec[7] == ITEM_BOTTLE && !bottleFull)
                {
                    bottleFull = 1;
                    openText(rec[11]);
                    return 1;
                }
                if (rec[7] == ITEM_BALLS && balls < 10)
                {
                    balls = 10;
                    openText(rec[11]);
                    return 1;
                }
            }
            /* someone with a minigame (the Ballplayer once you've won his challenge; the Trail Cyclist once
               he's given you the bike) */
            if (rec[13] && !(rec[7] && !hasItem(rec[7])) && (!rec[12] || (pins & (1 << (rec[12] - 1)))))
            {
                miniOffer(rec[13], rec[14]);
                return 1;
            }
            /* someone with a challenge: after anything they have to give you, and between their other lines */
            if (rec[12] && !(rec[7] && !hasItem(rec[7])) && !chalOn && npcTalk[k] >= rec[3])
            {
                npcTalk[k] = 0;
                chalOffer(rec[12] - 1);
                return 1;
            }
            if (!rec[3]) return 1; /* nothing else to say (mid-challenge) */
            /* someone handing you something (the Archivist's Gas Lantern) */
            if (rec[7] && !hasItem(rec[7]))
                npcTalk[k] = rec[6];
            openText(rec[2] + npcTalk[k]);
            if (rec[7] && npcTalk[k] == rec[6] && !hasItem(rec[7]))
                gotItem(rec[7]);
            npcTalk[k]++;
            if (npcTalk[k] >= rec[3] && !rec[12]) npcTalk[k] = 0; /* (a challenge-giver offers it next) */
            return 1;
        }
    if (fx < 0 || fy < 0 || fx >= MAP_PX_W || fy >= MAP_PX_H)
        return 0;
    for (k = 0; k < chests[0]; k++)
    {
        rec = chestRec(k);
        if (rec[0] == (fx >> 4) && rec[1] == (fy >> 4))
        {
            t = rec[2];
            if (chestOpen[t >> 3] & (1 << (t & 7)))
                return 0; /* already open: swing instead */
            chestOpen[t >> 3] |= 1 << (t & 7);
            sfx(SFX_JING);
            if (rec[3] == 0)
            {
                coins += rec[4];
                if (coins > 999) coins = 999;
            }
            else if (rec[3] == 1 && hpMax < 20)
            {
                hpMax += 2;
                hp = hpMax;
            }
            else if (rec[3] == EFFECT_KEY)
                keys[roomArea]++;
            else if (rec[3] == EFFECT_ITEM)
                gotItem(rec[4]);
            else if (rec[3] == EFFECT_PIECE)
            {
                chestTiles(k, 0);
                gotPiece();
                return 1;
            }
            chestTiles(k, 0);
            openText(rec[5]);
            return 1;
        }
    }
    if (tryUnlock(fx, fy) == 2)
    {
        openTextAt((u8 *)&sys_txt + TXT_LOCKED);
        return 1;
    }
    /* the Mural Hunt: A at a mural you haven't found yet */
    if (chalAt(CH_TAG, fx, fy)) return 1;
    t = wld[WLD_GRID + (fy >> 4) * COL_STRIDE + (fx >> 4)];
    if (!t)
        return 0;
    loreMark(t - 1);
    /* Stone House pinball: one coin a game */
    for (k = 0; k < ARCADE_N; k++)
        if (arcadeAt[k * 2] == curMap && arcadeAt[k * 2 + 1] == t - 1)
        {
            miniOffer(MINI_PIN, t - 1);
            askOpt[0] = "PLAY (1 COIN)";
            askOpt[1] = "WALK AWAY";
            return 1;
        }
    openText(t - 1);
    return 1;
}

/* Step onto a door: go in, or get turned back with a message. */
void useDoor(u8 tx, u8 ty)
{
    u8 k;
    u8 *d;
    for (k = 0; k < doors[0]; k++)
    {
        d = doors + 1 + k * 4;
        if (d[0] != tx || d[1] != ty) continue;
        if (d[2] != 255 && (d[3] == DOOR_OPEN || (d[3] == DOOR_CARKEY && hasCarkey) || (d[3] == DOOR_KEYSTONES && keystones == 15)))
        {
            retMap = curMap;
            retX = tx;
            retY = ty + 1;
            px = 128;
            py = 8 * 16 + 6;
            heroDir = 1;
            enterMap(d[2]);
            if (roomArea) dentMap = d[2];
            return;
        }
        px = tx * 16 + 8;
        py = (ty + 1) * 16 + 8;
        heroDir = 0;
        updateCamera();
        openTextAt((u8 *)&sys_txt + (d[3] == DOOR_KEYSTONES ? TXT_KEYSTONES : d[3] == DOOR_CARKEY ? TXT_CARKEY : TXT_LATER));
        return;
    }
}

/* Doors and the way out of interiors are checked under the hero's feet. */
u16 lastCell = 0xFFFF; /* doors and exits fire when you step onto them, not while knocked onto one */
u16 lastPad;
extern u16 padSeen;
extern void rcqNmi(void);

void checkCells(void)
{
    u8 c, m;
    u16 at;
    if (px < 0 || py < 0 || px >= MAP_PX_W || py >= MAP_PX_H) return;
    at = (py >> 4) * COL_STRIDE + (px >> 4);
    if (at == lastCell) return;
    lastCell = at;
    c = colGrid[at];
    if (c == CELL_DOOR)
        useDoor(px >> 4, py >> 4);
    else if (c == CELL_EXIT && retMap != 255)
    {
        px = retX * 16 + 8;
        py = retY * 16 + 8;
        heroDir = 0;
        m = retMap;
        retMap = 255;
        dentMap = 255;
        enterMap(m);
    }
}

/* After "The fog takes you...": wake by the fountain in Capitol Square (web: 7,8 facing left). */
void respawn(void)
{
    dying = 0;
    hp = hpMax;
    inv = 60;
    if (dentMap != 255)
    {
        /* "You come to at the entrance." */
        px = 128;
        py = 8 * 16 + 6;
        heroDir = 1;
        enterMap(dentMap);
        return;
    }
    retMap = 255;
    px = 7 * 16 + 8;
    py = 8 * 16 + 8;
    heroDir = 2;
    enterMap(MAP_CAPITOL);
}

int main(void)
{
    u16 pad, pressed, i;
    s16 dx, dy, spd;
    u8 b, cont;

    setMode(BG_MODE1, BG3_MODE1_PRIORITY_HIGH);
    bgSetGfxPtr(0, VRAM_BG1_CHR);
    bgSetMapPtr(0, VRAM_BG1_MAP, SC_64x64);
    bgSetGfxPtr(2, VRAM_BG3_CHR);
    bgSetMapPtr(2, VRAM_BG3_MAP, SC_32x32);
    bgSetDisable(1);

    /* 8x8 small sprites for the HUD and pickups, 16x16 large for everyone else */
    /* the hero's first frame goes in with his palette; heroFrame streams the rest */
    oamInitGfxSet(&hero_chr, 128, &hero_pal, (&hero_palend - &hero_pal), 0, VRAM_OBJ_CHR, OBJ_SIZE8_L16);
    heroShown = 255;
    heroPend = 0;
    for (i = 0; i < 128; i++)
    {
        oamShown[i] = 1;
        sprHide(i);
    }

    /* RAM is not cleared at power-on: set every variable before use. */
    heroDir = 0;
    step = 0;
    mapW = 32;
    mapH = 22;
    frameCount = 0;
    npcCount = 0;
    boxState = BOX_CLOSED;
    boxDirty = 0;
    boxTop = 20;
    boxLine = 0;
    boxCol = 0;
    retMap = 255;
    patchCount = 0;
    shopReturn = 0;
    for (i = 0; i < sizeof(chestOpen); i++)
        chestOpen[i] = 0;
    for (i = 0; i < MAX_SHOP_ITEMS; i++)
    {
        shopOwned[0][i] = 0;
        shopOwned[1][i] = 0;
    }
    audioInit();
    lightInit();
    weatherInit();
    combatInit();
    dungeonInit();
    gearInit();
    setBrightness(0); /* forced blank while loading */

    /* Enemies, the paddle and the HUD: OBJ tiles 128-255, sprite palettes 5-7. */
    dmaCopyVram(&combat_chr, VRAM_OBJ_CHR + 128 * 16, (&combat_chrend - &combat_chr));
    dmaCopyCGram(&combat_pal, 128 + 5 * 16, 3 * 16 * 2);
    /* items (kegs, icons, keystones, heart containers, explosions): second OBJ name table, tile 256 */
    dmaCopyVram(&items_chr, VRAM_OBJ_CHR + 0x1000, (&items_chrend - &items_chr));

    /* Font, and an empty text layer. */
    dmaCopyVram(&font_chr, VRAM_BG3_CHR, (&font_chrend - &font_chr));
    for (i = 0; i < BOX_ROWS * 32; i++)
        boxMap[i] = 0;
    for (i = 0; i < 4; i++)
        dmaCopyVram((u8 *)boxMap, VRAM_BG3_MAP + i * BOX_ROWS * 32, BOX_ROWS * 32 * 2);
    bgSetScroll(2, 0, 0);

    /* the title screen: continue a saved game, or name a new hero */
    saveInit();
    nmiSet(rcqNmi); /* every VBlank: catch taps in long frames */
    padSeen = 0;
    cont = runTitle();
    setBrightness(0);
    for (i = 0; i < 4; i++)
        dmaCopyVram((u8 *)boxMap, VRAM_BG3_MAP + i * BOX_ROWS * 32, BOX_ROWS * 32 * 2);
    lightDirty = 1;

    if (!cont)
    {
        curMap = MAP_START;
        px = MAP_PX_W / 2; /* the middle of the Fan */
        py = MAP_PX_H / 2;
    }
    rcqLoadMap(curMap);
    settle();
    updateCamera();
    drawHero();
    drawNpcs();
    combatDraw();
    hudDraw();
    songForMap();
    WaitForVBlank();
    for (spd = 1; spd <= 15; spd++)
    {
        setBrightness(spd);
        WaitForVBlank();
    }

    saveGame(); /* the slot is yours from the start */
    if (!cont) openIntro();
    else dungeonEntered();
    padSeen = 0;
    lastPad = 0xFFFF; /* nothing held at the start counts as a press */
    while (1)
    {
        frameCount++;
        playTick();
        /* new presses since our last look (not padsDown, which only holds a press for one frame:
           when a busy frame runs long, that press would be lost) */
        pad = padsCurrent(0);
        /* a tap that came and went during a long frame still counts (src/padnmi.asm) */
        pressed = (pad | padSeen) & ~lastPad;
        padSeen = 0;
        lastPad = pad;

        if (boxState != BOX_CLOSED)
        {
            updateText(pressed);
            step = 0;
            if (boxState == BOX_CLOSED && endingPending)
            {
                endingPending = 0;
                runEnding();
            }
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
        else if (curSong == SONG_BOSS && !bossOn && !endingPending)
        {
            songForMap(); /* the boss is down and its last words are read: back to this place's song */
        }
        else if ((pressed & KEY_START) && !heroBusy() && !charging && !dying)
        {
            openMenu();
            step = 0;
        }
        else
        {
            combatUpdate(pad, pressed);
            dungeonUpdate();
            lifeUpdate();
            chalUpdate();
            frogTick();
            dx = 0;
            dy = 0;
            /* 1.5 px per frame like the web game's walk; slower while charging a spin */
            spd = (frameCount & 1) ? 2 : 1;
            if (charging) spd = (frameCount % 5) ? 1 : 0;
            if (riding) spd = 2;
            updDash();
            updBalls();
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
                b = 0;
                if (dx && !blocked(px + dx, py)) { px += dx; b = 1; }
                else if (dx) tryUnlock(px + dx * 8, py);
                if (dy && !blocked(px, py + dy)) { py += dy; b = 1; }
                else if (dy) tryUnlock(px, py + dy * 8);
                /* leaning on a block in one direction pushes it (web tryPush) */
                if (!b && (!dx || !dy)) tryPush();
                else notPushing();
                checkExits();
                checkCells();
            }
            else if (!charging)
                step = 0;
            updateCamera();
        }

        drawHero();
        drawNpcs();
        lifeDraw();
        chalDraw();
        combatDraw();
        dungeonDraw();
        drawBalls();
        drawBike();
        hudDraw();
        lightFrame();
        weatherFrame();
        WaitForVBlank();
        flushText();
    }
    return 0;
}
