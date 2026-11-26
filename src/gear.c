/* Gear for B from around town (web give() and useItem()): Baseballs, Running Shoes, the
   Bicycle and Ginger Beer, plus the item menu on START. */
#include "game.h"

u8 hasBalls, hasShoes, hasBike, hasBottle, hasBib;
u8 bottleFull, balls, capBalls, capPowder, riding, dash;
u8 hudBalls, hudBottle;

#define MAX_BALL 3
u8 ballOn[MAX_BALL], ballLife[MAX_BALL];
s16 ballX[MAX_BALL], ballY[MAX_BALL], ballVX[MAX_BALL], ballVY[MAX_BALL];

void gearInit(void)
{
    u8 i;
    hasBalls = 0;
    hasShoes = 0;
    hasBike = 0;
    hasBottle = 0;
    hasBib = 0;
    bottleFull = 0;
    balls = 0;
    capBalls = 30;
    capPowder = 10;
    riding = 0;
    dash = 0;
    hudBalls = 255;
    hudBottle = 255;
    for (i = 0; i < MAX_BALL; i++) ballOn[i] = 0;
}

/* ---------- Baseballs: thrown straight ahead at 3.4 px a frame ---------- */

void throwBall(void)
{
    u8 i;
    if (!balls)
    {
        openTextAt((u8 *)&sys_txt + TXT_NOBALLS);
        return;
    }
    for (i = 0; i < MAX_BALL; i++)
        if (!ballOn[i])
        {
            balls--;
            sfx(SFX_SWING);
            ballOn[i] = 1;
            ballLife[i] = 70;
            ballX[i] = (px + dirX[heroDir] * 6) * 16;
            ballY[i] = (py - 4 + dirY[heroDir] * 6) * 16;
            ballVX[i] = dirX[heroDir] * 54;
            ballVY[i] = dirY[heroDir] * 54;
            return;
        }
}

/* balls fly over water and gaps (web BALLSOLID), not through walls */
u8 ballWall(s16 x, s16 y)
{
    u8 c;
    if (x < 0 || y < 0 || x >= MAP_PX_W || y >= MAP_PX_H) return 1;
    c = colGrid[(y >> 4) * COL_STRIDE + (x >> 4)];
    return c == CELL_SOLID || c == CELL_COUNTER || c == CELL_BUSH || c == CELL_POST || c == CELL_LOCK || c == CELL_BRAZIER || c == CELL_SPROUT;
}

void updBalls(void)
{
    u8 i, k;
    s16 x, y;
    for (i = 0; i < MAX_BALL; i++)
    {
        if (!ballOn[i]) continue;
        ballX[i] += ballVX[i];
        ballY[i] += ballVY[i];
        x = ballX[i] >> 4;
        y = ballY[i] >> 4;
        if (!--ballLife[i] || ballWall(x, y))
        {
            ballOn[i] = 0;
            continue;
        }
        for (k = 0; k < MAX_EN; k++)
            if (enAlive[k] && ABS((enX[k] >> 4) - x) < 9 && ABS((enY[k] >> 4) - 4 - y) < 10)
            {
                hitEnemy(k, 1, ballVX[i] * 6 / 10, ballVY[i] * 6 / 10);
                ballOn[i] = 0;
                break;
            }
        if (ballOn[i] && bossOn && ABS((bossX >> 4) - x) < 13 && ABS((bossY >> 4) - 4 - y) < 14 && bossHit(1, 0))
            ballOn[i] = 0;
    }
}

void drawBalls(void)
{
    u8 i;
    s16 sx, sy;
    for (i = 0; i < MAX_BALL; i++)
    {
        sx = (ballX[i] >> 4) - camX - 8;
        sy = (ballY[i] >> 4) - camY - 8;
        if (!ballOn[i] || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H) sprHide(OAM_BALLS + i);
        else sprPut(OAM_BALLS + i, 1, sx, sy, 3, 0, OBJ_I_BALL, PAL_ITEM);
    }
}

/* ---------- Running Shoes: a 20-frame dash, 3 px a frame; nothing hurts you mid-dash ---------- */

void updDash(void)
{
    u8 k;
    s16 ox = px, oy = py, nx, ny;
    if (!dash) return;
    dash--;
    nx = px + dirX[heroDir] * 3;
    ny = py + dirY[heroDir] * 3;
    if (!blocked(nx, py)) px = nx;
    if (!blocked(px, ny)) py = ny;
    if (px == ox && py == oy) dash = 0; /* ran into a wall */
    for (k = 0; k < MAX_EN; k++)
        if (enAlive[k] && ABS((enX[k] >> 4) - px) < 12 && ABS((enY[k] >> 4) - py) < 12)
            hitEnemy(k, 1, dirX[heroDir] * 48, dirY[heroDir] * 48);
    checkExits();
    checkCells();
}

/* ---------- the Bicycle: outdoors only, about 2 px a frame ---------- */

void drawBike(void)
{
    s16 sx = px - camX - 8, sy = py - camY - 4;
    if (!riding || dying)
    {
        sprHide(OAM_BIKE);
        return;
    }
    sprPut(OAM_BIKE, 1, sx, sy, 3, heroDir == 2, heroDir >= 2 ? OBJ_I_BIKESIDE : OBJ_I_BIKEFRONT, PAL_ITEM);
}

/* ---------- B ---------- */

void useGear(void)
{
    if (eqItem == ITEM_BALLS) throwBall();
    else if (eqItem == ITEM_SHOES)
    {
        if (!dash)
        {
            dash = 20;
            sfx(SFX_SWING);
        }
    }
    else if (eqItem == ITEM_BIKE)
    {
        if (mapW != 32) openTextAt((u8 *)&sys_txt + TXT_NOBIKE);
        else riding = !riding;
    }
    else if (eqItem == ITEM_BOTTLE)
    {
        if (bottleFull)
        {
            bottleFull = 0;
            hp = hpMax;
            sfx(SFX_HEART);
            openTextAt((u8 *)&sys_txt + TXT_DRINK);
        }
        else openTextAt((u8 *)&sys_txt + TXT_EMPTY);
    }
}

void gotGear(u8 item)
{
    if (item == ITEM_BALLS)
    {
        hasBalls = 1;
        if (balls < 15) balls = 15;
    }
    else if (item == ITEM_SHOES) hasShoes = 1;
    else if (item == ITEM_BIKE) hasBike = 1;
    else if (item == ITEM_BOTTLE)
    {
        hasBottle = 1;
        bottleFull = 1;
    }
}

/* the HUD count beside the B icon: kegs and baseballs */
void hudCount(u8 n)
{
    if (n >= 10) sprPut(OAM_BICON + 1, 0, 137, 12, 3, 0, HUD_DIGIT + n / 10, PAL_HUD);
    else sprHide(OAM_BICON + 1);
    sprPut(OAM_BICON + 2, 0, 145, 12, 3, 0, HUD_DIGIT + n % 10, PAL_HUD);
}

/* ---------- the item menu (START): pick what B does ---------- */

static const char *const itemName[13] = {"", "Grappling Hook", "Blasting Powder", "Gas Lantern", "Cast-Iron Shield", "The Gold-Bug",
                                          "Watering Can", "Baseballs", "Running Shoes", "Bicycle", "Ginger Beer", "Fishing Rod", "Camera"};
#define MENU_CARDS 99 /* the last lines of the menu: your landmark cards, your photos */
#define MENU_ALBUM 98
/* the web game's menu order */
static const u8 menuOrder[11] = {ITEM_LANTERN, ITEM_BALLS, ITEM_POWDER, ITEM_BUG, ITEM_SHOES, ITEM_HOOK, ITEM_CAN, ITEM_BIKE, ITEM_BOTTLE, ITEM_ROD, ITEM_CAMERA};
u8 menuList[13], menuN, menuCur, menuTop;

void drawMenu(void)
{
    u8 r, j, it;
    boxDrawFrame();
    boxText(1, 2, (u8 *)"ITEMS", FONT_GOLD);
    for (r = 0; r < 4; r++)
    {
        j = menuTop + r;
        if (j >= menuN) break;
        it = menuList[j];
        boxPut(2 + r, 2, j == menuCur ? FONT_GOLD + '>' - 32 : FILL);
        if (it == MENU_ALBUM)
        {
            boxText(2 + r, 3, (u8 *)"Photo Album", FONT_WHITE);
            boxNum(2 + r, 29, PHOTO_COUNT);
            boxPut(2 + r, 27, FONT_GOLD + '/' - 32);
            boxNum(2 + r, 26, photoCount());
            continue;
        }
        if (it == MENU_CARDS)
        {
            boxText(2 + r, 3, (u8 *)"Landmark Cards", FONT_WHITE);
            boxNum(2 + r, 29, CARD_COUNT);
            boxPut(2 + r, 27, FONT_GOLD + '/' - 32);
            boxNum(2 + r, 26, cardCount());
            continue;
        }
        boxText(2 + r, 3, (u8 *)itemName[it], it == eqItem ? FONT_GOLD : FONT_WHITE);
        if (it == ITEM_POWDER) boxNum(2 + r, 29, powder);
        else if (it == ITEM_BALLS) boxNum(2 + r, 29, balls);
        else if (it == ITEM_BOTTLE) boxTextRight(2 + r, 29, bottleFull ? (u8 *)"FULL" : (u8 *)"EMPTY", FONT_GOLD);
    }
    boxText(6, 2, (u8 *)"A: SET ON B   START: CLOSE", FONT_GOLD);
    boxState = BOX_MENU;
}

void openMenu(void)
{
    u8 i;
    menuN = 0;
    menuCur = 0;
    for (i = 0; i < 11; i++)
        if (hasItem(menuOrder[i]))
        {
            if (menuOrder[i] == eqItem) menuCur = menuN;
            menuList[menuN++] = menuOrder[i];
        }
    menuList[menuN++] = MENU_CARDS;
    if (hasCamera) menuList[menuN++] = MENU_ALBUM;
    menuTop = menuCur > 3 ? menuCur - 3 : 0;
    boxTop = (py - camY > 120) ? 0 : 20;
    drawMenu();
}

void updateMenu(u16 pressed)
{
    if (pressed & KEY_UP && menuCur > 0)
    {
        menuCur--;
        sfx(SFX_MENU);
        if (menuCur < menuTop) menuTop = menuCur;
        drawMenu();
    }
    else if (pressed & KEY_DOWN && menuCur + 1 < menuN)
    {
        menuCur++;
        sfx(SFX_MENU);
        if (menuCur >= menuTop + 4) menuTop = menuCur - 3;
        drawMenu();
    }
    else if ((pressed & KEY_A) && menuList[menuCur] == MENU_CARDS)
        openCards();
    else if ((pressed & KEY_A) && menuList[menuCur] == MENU_ALBUM)
        runAlbum();
    else if (pressed & KEY_A)
    {
        eqItem = menuList[menuCur];
        if (eqItem != ITEM_BIKE) riding = 0;
        closeText();
    }
    else if (pressed & (KEY_START | KEY_B))
        closeText();
}
