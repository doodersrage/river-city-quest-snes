/* River City Quest (SNES): fishing, rainy-night frogs and landmark cards (web systems.js FISH, pickFish,
   fishUpdate, drawBobber, drawFishUI, frogTick, CARDS, awardCard; people.js naturalist). */
#include <snes.h>
#include "game.h"

#define OAM_BOBBER 98
#define BOX_FISH 7
#define BOX_CARDS 8

extern u8 boxState, boxTop;
extern u16 boxMap[];
void boxDrawFrame(void);
void boxFillLines(void);
void boxPut(u8 row, u8 col, u16 tile);
void boxText(u8 row, u8 col, u8 *s, u16 font);
void boxTextRight(u8 row, u8 right, u8 *s, u16 font);
void boxNum(u8 row, u8 right, u16 v);
extern const s8 dirX[], dirY[];

u8 hasRod, frogs, frogsDone;
u16 cards;
u8 fishN[FISH_COUNT];
u8 cardReturn;
u8 hasCamera, photoGot[(PHOTO_COUNT + 7) / 8];
static u8 camTxt[120];

/* the line: phase 0 waiting, 1 a bite, 2 reeling */
u8 fishPh, fishSp, fishRun;
u16 fishT, fishDist, fishTen;  /* distance and tension in 1/256 (web 0-72, 0-100) */
s16 fishX, fishY;

u8 cardCount(void)
{
    u8 k, n = 0;
    for (k = 0; k < CARD_COUNT; k++)
        if (cards & (1 << k)) n++;
    return n;
}

/* ---------- fishing ---------- */
static void fishBox(void)
{
    u8 c, w;
    boxFillLines();
    if (fishPh == 0) boxText(3, 2, (u8 *)"Waiting for a bite...", FONT_WHITE);
    else if (fishPh == 1) boxText(3, 2, (u8 *)"BITE! Press A!", FONT_GOLD);
    else
    {
        boxText(2, 2, (u8 *)"Tap A to reel", FONT_GOLD);
        if (fishRun && (frameCount & 8)) boxTextRight(2, 29, (u8 *)"RUNNING!", FONT_GOLD);
        boxText(3, 2, (u8 *)"FISH", FONT_WHITE);
        boxText(4, 2, (u8 *)"LINE", FONT_WHITE);
        /* how close the fish is, and how tight the line */
        w = fishDist >= 72 * 256 ? 0 : 22 - (u8)(((u16)(fishDist >> 8) * 22) / 72);
        for (c = 0; c < 22; c++) boxPut(3, 7 + c, c < w ? FONT_GOLD + '=' - 32 : FONT_WHITE + '.' - 32);
        w = fishTen >= 100 * 256 ? 22 : (u8)(((u16)(fishTen >> 8) * 22) / 100);
        for (c = 0; c < 22; c++) boxPut(4, 7 + c, c < w ? (w > 16 ? FONT_WHITE + '!' - 32 : FONT_GOLD + '=' - 32) : FONT_WHITE + '.' - 32);
    }
    boxText(6, 2, (u8 *)"B: stop", FONT_WHITE);
}

void fishCast(void)
{
    s16 fx = px + dirX[heroDir] * 20, fy = py + dirY[heroDir] * 20;
    u8 c, k;
    c = (fx < 0 || fy < 0 || fx >= MAP_PX_W || fy >= MAP_PX_H) ? 0 : colGrid[(fy >> 4) * COL_STRIDE + (fx >> 4)];
    if (c != CELL_WATER && c != CELL_RIVER)
    {
        openTextAt((u8 *)&sys_txt + TXT_FISHFACE);
        return;
    }
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k] && ABS((enX[k] >> 4) - px) < 60 && ABS((enY[k] >> 4) - py) < 60)
        {
            openTextAt((u8 *)&sys_txt + TXT_FISHBUSY);
            return;
        }
    fishX = fx;
    fishY = fy;
    fishPh = 0;
    fishT = 80 + rnd() % 160;
    if (hasLure) fishT = (fishT * 11) / 20; /* the Golden Lure: bites come sooner */
    sfx(SFX_SWING);
    boxTop = (py - camY > 120) ? 0 : 20;
    boxDrawFrame();
    boxText(1, 2, (u8 *)"Fishing", FONT_GOLD);
    fishBox();
    boxState = BOX_FISH;
}

/* web pickFish: weights x20 */
static u8 pickFish(void)
{
    u16 w[FISH_COUNT], t = 0, r;
    u8 k, dark = nightLevel() > 2;
    w[0] = 100;                              /* bluegill */
    w[1] = 80;                               /* bass */
    w[2] = dark ? 120 : 20;                  /* catfish feed at night */
    w[3] = wx == 1 ? 100 : 6;                /* shad: rain gets them moving */
    w[4] = mapRiver[curMap] ? (dark ? 18 : 9) * (hasLure ? 3 : 1) : 0; /* sturgeon, in the James */
    for (k = 0; k < FISH_COUNT; k++) t += w[k];
    r = rnd() % t;
    for (k = 0; k < FISH_COUNT; k++)
    {
        if (r < w[k]) return k;
        r -= w[k];
    }
    return 0;
}

static void fishEnd(u16 txt)
{
    sprHide(OAM_BOBBER);
    closeText();
    sfx(SFX_FAIL);
    openTextAt((u8 *)&sys_txt + txt);
}

void fishUpdate(u16 pressed)
{
    u8 s;
    if (fishT) fishT--;
    if (pressed & KEY_B)
    {
        fishEnd(TXT_FISHREEL);
        return;
    }
    if (fishPh == 0)
    {
        if (pressed & KEY_A)
        {
            fishEnd(TXT_FISHEARLY);
            return;
        }
        if (!fishT)
        {
            fishPh = 1;
            fishT = 28;
            sfx(SFX_CHAL);
            fishBox();
        }
    }
    else if (fishPh == 1)
    {
        if (pressed & KEY_A)
        {
            fishPh = 2;
            fishSp = pickFish();
            fishDist = 40 * 256;
            fishTen = 18 * 256;
            fishRun = 0;
            sfx(SFX_MENU);
        }
        else if (!fishT)
        {
            fishEnd(TXT_FISHSTOLE);
            return;
        }
    }
    else
    {
        s = fishStr[fishSp];
        if (fishRun) fishRun--;
        else if (rnd() % 1000 < (u16)s * 12 / 10) fishRun = 35 + rnd() % 40;
        fishDist += (fishRun ? 33 : 13) * s / 10;   /* web .13 or .05 x strength */
        fishTen = fishTen > (fishRun ? 51 : 192) ? fishTen - (fishRun ? 51 : 192) : 0;
        if (pressed & KEY_A)
        {
            fishDist = fishDist > 819 ? fishDist - 819 : 0;
            fishTen += fishRun ? 3840 : 2048;
            sfx(SFX_TALK);
        }
        if (fishTen >= 100 * 256)
        {
            fishEnd(TXT_FISHSNAP);
            return;
        }
        if (fishDist >= 72 * 256)
        {
            fishEnd(TXT_FISHAWAY);
            return;
        }
        if (!fishDist)
        {
            /* landed */
            sprHide(OAM_BOBBER);
            closeText();
            if (fishN[fishSp] < 255) fishN[fishSp]++;
            coins = coins + fishCoins[fishSp] > 999 ? 999 : coins + fishCoins[fishSp];
            sfx(SFX_JING);
            if (fishSp == FISH_COUNT - 1 && !(cards & (1 << CARD_STURGEON)))
            {
                cards |= 1 << CARD_STURGEON;
                openTextAt((u8 *)&sys_txt + TXT_FISHCARD);
            }
            else openTextAt((u8 *)&sys_txt + fishTxt[fishSp]);
            return;
        }
    }
    if (fishPh == 2) fishBox();
    /* the bobber bobs, dips on a bite */
    sprPut(OAM_BOBBER, 0, fishX - camX - 2, fishY - camY - 2 + ((frameCount >> 4) & 1) + (fishPh == 1 ? 3 : fishPh == 2 ? 2 : 0), 3, 0, HUD_BOBBER, PAL_HUD);
}

/* ---------- frogs: out on rainy days near the water (web frogTick) ---------- */
void frogTick(void)
{
    u8 k, n = 0, i, tx, ty, a;
    s16 nx, ny;
    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k] == PK_FROG)
        {
            n++;
            if (pkHop[k]) pkHop[k]--;
            else
            {
                pkHop[k] = 50 + rnd() % 70;
                a = rnd() & 3;
                nx = pkX[k] + dirX[a] * 12;
                ny = pkY[k] + dirY[a] * 12;
                if (nx > 8 && ny > 8 && nx < MAP_PX_W - 8 && ny < MAP_PX_H - 8 && !solidAt(nx, ny))
                {
                    pkX[k] = nx;
                    pkY[k] = ny;
                }
            }
        }
    if ((frameCount % 90) || mapDark[curMap] || wx != 1 || wxK < 128 || frogsDone || n >= 3) return;
    for (i = 0; i < 40; i++)
    {
        tx = 1 + rnd() % (mapW - 2);
        ty = 1 + rnd() % (mapH - 2);
        if (colGrid[ty * COL_STRIDE + tx] != CELL_OPEN) continue;
        if (colGrid[ty * COL_STRIDE + tx + 1] != CELL_WATER && colGrid[ty * COL_STRIDE + tx - 1] != CELL_WATER &&
            colGrid[(ty + 1) * COL_STRIDE + tx] != CELL_WATER && colGrid[(ty - 1) * COL_STRIDE + tx] != CELL_WATER) continue;
        nx = tx * 16 + 8;
        ny = ty * 16 + 8;
        if (ABS(nx - px) < 48 && ABS(ny - py) < 48) continue;
        dropPickup(nx, ny, PK_FROG, 255);
        for (k = 0; k < MAX_PK; k++)
            if (pkKind[k] == PK_FROG && pkX[k] == nx && pkY[k] == ny)
            {
                pkLife[k] = 1500;
                pkHop[k] = 30 + rnd() % 40;
            }
        return;
    }
}

void gotFrog(void)
{
    if (frogs < 250) frogs++;
    sfx(SFX_CHAL);
    openTextAt((u8 *)&sys_txt + (frogs >= 5 ? TXT_FROG5 : frogs == 4 ? TXT_FROG4 : frogs == 3 ? TXT_FROG3 : frogs == 2 ? TXT_FROG2 : TXT_FROG1));
}

/* ---------- landmark cards ---------- */
void gotPackCard(void)
{
    u8 k, n = 0, pick;
    for (k = 0; k < PACK_COUNT; k++)
        if (!(cards & (1 << packCard[k]))) n++;
    if (!n) return;
    pick = rnd() % n;
    for (k = 0; k < PACK_COUNT; k++)
        if (!(cards & (1 << packCard[k])))
        {
            if (!pick)
            {
                cards |= 1 << packCard[k];
                openTextAt((u8 *)&sys_txt + cardGotTxt[packCard[k]]);
                return;
            }
            pick--;
        }
}

u8 packSoldOut(void)
{
    u8 k;
    for (k = 0; k < PACK_COUNT; k++)
        if (!(cards & (1 << packCard[k]))) return 0;
    return 1;
}

u8 cardCur, cardTop;

void drawCards(void)
{
    u8 r, j;
    boxDrawFrame();
    boxText(1, 2, (u8 *)"LANDMARK CARDS", FONT_GOLD);
    boxNum(1, 29, CARD_COUNT);
    boxPut(1, 27, FONT_GOLD + '/' - 32);
    boxNum(1, 26, cardCount());
    for (r = 0; r < 4; r++)
    {
        j = cardTop + r;
        if (j >= CARD_COUNT) break;
        boxPut(2 + r, 2, j == cardCur ? FONT_GOLD + '>' - 32 : FILL);
        boxText(2 + r, 3, (cards & (1 << j)) ? (u8 *)cardName[j] : (u8 *)"???", (cards & (1 << j)) ? FONT_WHITE : FONT_GOLD);
    }
    boxText(6, 2, (u8 *)"A: READ   B: BACK", FONT_GOLD);
    boxState = BOX_CARDS;
}

void openCards(void)
{
    cardCur = 0;
    cardTop = 0;
    drawCards();
}

void updateCards(u16 pressed)
{
    if ((pressed & KEY_UP) && cardCur > 0)
    {
        cardCur--;
        sfx(SFX_MENU);
        if (cardCur < cardTop) cardTop = cardCur;
        drawCards();
    }
    else if ((pressed & KEY_DOWN) && cardCur + 1 < CARD_COUNT)
    {
        cardCur++;
        sfx(SFX_MENU);
        if (cardCur >= cardTop + 4) cardTop = cardCur - 3;
        drawCards();
    }
    else if (pressed & KEY_A)
    {
        cardReturn = 1; /* back to the list when it's read */
        openTextAt((u8 *)&sys_txt + ((cards & (1 << cardCur)) ? cardTxt[cardCur] : TXT_CARDNONE));
    }
    else if (pressed & (KEY_B | KEY_START))
        closeText();
}

/* ---------- the camera (web snapPhoto): the nearest captioned landmark within 150 px goes in the album ---------- */
u8 photoCount(void)
{
    u8 k, n = 0;
    for (k = 0; k < PHOTO_COUNT; k++)
        if (photoGot[k >> 3] & (1 << (k & 7))) n++;
    return n;
}

static char *camPut(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    return d;
}

void snapPhoto(void)
{
    u8 i, best = 255, k, isNew, n;
    s16 dx, dy;
    u16 d2, bd = 0xFFFF;
    char *d;
    sfx(SFX_MENU);
    sfx(SFX_COIN);
    for (i = 0; i < PHOTO_AT; i++)
    {
        if (photoAt[i * 4] != curMap) continue;
        dx = (photoAt[i * 4 + 2] << 1) - px;
        dy = (photoAt[i * 4 + 3] << 1) - py;
        if (ABS(dx) >= 150 || ABS(dy) >= 150) continue;
        dx >>= 2;
        dy >>= 2;
        d2 = dx * dx + dy * dy;
        if (d2 < 1406 && d2 < bd)
        {
            bd = d2;
            best = photoAt[i * 4 + 1];
        }
    }
    if (best == 255)
    {
        openTextAt((u8 *)&sys_txt + TXT_CAMNONE);
        return;
    }
    k = best;
    isNew = !(photoGot[k >> 3] & (1 << (k & 7)));
    photoGot[k >> 3] |= 1 << (k & 7);
    n = photoCount();
    /* "New photo: (n/30)", then the caption */
    d = camPut((char *)camTxt, "Camera");
    *d++ = 0;
    d = camPut(d, isNew ? "New photo! (" : "Retook it. (");
    if (n >= 10) *d++ = '0' + n / 10;
    *d++ = '0' + n % 10;
    *d++ = '/';
    *d++ = '0' + PHOTO_COUNT / 10;
    *d++ = '0' + PHOTO_COUNT % 10;
    *d++ = ')';
    *d++ = 0x0A;
    d = camPut(d, photoWrap[k]);
    *d++ = 0;
    *d = 0;
    openTextAt(camTxt);
}

void fishReset(void)
{
    u8 k;
    hasCamera = 0;
    for (k = 0; k < (PHOTO_COUNT + 7) / 8; k++) photoGot[k] = 0;
    hasRod = frogs = frogsDone = 0;
    cards = 0;
    cardReturn = 0;
    for (k = 0; k < FISH_COUNT; k++) fishN[k] = 0;
}
