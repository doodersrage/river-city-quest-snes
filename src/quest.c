/* River City Quest (SNES), stage 15a: the quest log, the journal and the bus map, from the START menu
   (web extras.js quests, goalScene, busTo, and the journal tab). Each is a list in the text box; reading an entry
   opens it as text and comes back to the list. */
#include <snes.h>
#include "game.h"

void boxDrawFrame(void);
void boxPut(u8 row, u8 col, u16 tile);
void boxText(u8 row, u8 col, u8 *s, u16 font);
void boxTextRight(u8 row, u8 right, u8 *s, u16 font);
void boxNum(u8 row, u8 right, u16 v);
u8 cardCount(void);
u8 photoCount(void);
u8 pinCount(void);
void enterMap(u8 i);
extern u8 cardReturn;
extern u8 hasCan;
extern u8 *txt;

u8 qCur, qTop, medals;

#define BOX_QUEST 9
#define BOX_JOUR 10
#define BOX_BUS 11

/* ---------- the next step of the story (web goalScene / goalEn) ---------- */
u8 goalStep(void)
{
    if (!hasLantern) return 0;
    if (!(keystones & 1)) return 1;
    if (!(keystones & 2)) return 2;
    if (!(keystones & 4)) return 3;
    if (!(keystones & 8))
    {
        if (hasCarkey) return 4;
        if (hasKayak) return 5;
        return tradeItem == 1 ? 6 : tradeItem == 2 ? 7 : tradeItem == 3 ? 8 : 9;
    }
    if (!gameWon) return 10;
    if (!(roomFlags[MAP_TREDEGAR_BOSS] & ROOM_BOSS)) return 11;
    if (!(roomFlags[MAP_CONSERV_BOSS] & ROOM_BOSS)) return 12;
    if (!bateauDone) return 13;
    if (!frogsDone) return 14;
    return 15;
}

/* the outdoor map the next step points to, 255 when the story's done */
u8 goalOwMap(void)
{
    u8 k = goalStep();
    return k < 15 ? goalOw[k] : 255;
}

static u8 bits8(u8 v)
{
    u8 n = 0;
    while (v)
    {
        n += v & 1;
        v >>= 1;
    }
    return n;
}

u8 medalCount(void)
{
    u8 n = (medals & 1) ? 1 : 0;   /* the slugger: three home runs in a round */
    if (miniBest[MINI_PIN] >= 8000) n++;
    if (miniBest[MINI_DERBY] && miniBest[MINI_DERBY] < raceGold[0]) n++;
    if (miniBest[MINI_TRAIL] && miniBest[MINI_TRAIL] < raceGold[1]) n++;
    return n;
}

/* ---------- the quest log ---------- */
#define Q_COUNT 15
static const char *const qName[Q_COUNT] = {
    "Find the keystones", "Unseal the tunnel", "Trade for a kayak", "Find the Carillon key", "Return the race bib",
    "Tredegar Iron Works", "The Conservatory", "Rebuild the bateau", "Count frogs in rain", "Neighborhood pins",
    "Heart pieces", "Landmark cards", "Photo album", "Mini-game medals", "What next?"};
static const char *const tradeName[4] = {"", "Watermelon", "Sheet Music", "River Map"};

/* a quest's count (have/of; of 0 for none) and whether it's done */
static u8 qHave, qOf;
static u8 questState(u8 k)
{
    qOf = 0;
    switch (k)
    {
    case 0: qHave = bits8(keystones); qOf = 4; return qHave >= 4;
    case 1: return gameWon;
    case 2: return hasKayak;
    case 3: return hasCarkey;
    case 4: return hasShoes;
    case 5: return (roomFlags[MAP_TREDEGAR_BOSS] & ROOM_BOSS) != 0;
    case 6: return (roomFlags[MAP_CONSERV_BOSS] & ROOM_BOSS) != 0;
    case 7: qHave = bits8(parts); if (qHave > 3) qHave = 3; qOf = 3; return bateauDone;
    case 8: qHave = frogs > 5 ? 5 : frogs; qOf = 5; return frogsDone;
    case 9: qHave = pinCount(); qOf = CHAL_COUNT; return qHave >= CHAL_COUNT;
    case 10: qHave = pieces; qOf = 8; return pieces >= 8;
    case 11: qHave = cardCount(); qOf = CARD_COUNT; return qHave >= CARD_COUNT;
    case 12: qHave = photoCount(); qOf = PHOTO_COUNT; return qHave >= PHOTO_COUNT;
    case 13: qHave = medalCount(); qOf = 4; return qHave >= 4;
    }
    return 0;
}

void drawQuests(void)
{
    u8 r, j, done, n = 0, c;
    boxDrawFrame();
    for (j = 0; j < Q_COUNT - 1; j++)
        if (questState(j)) n++;
    boxText(1, 2, (u8 *)"QUEST LOG", FONT_GOLD);
    boxNum(1, 29, Q_COUNT - 1);
    boxPut(1, 27, FONT_GOLD + '/' - 32);
    boxNum(1, 26, n);
    for (r = 0; r < 4; r++)
    {
        j = qTop + r;
        if (j >= Q_COUNT) break;
        boxPut(2 + r, 2, j == qCur ? FONT_GOLD + '>' - 32 : FILL);
        done = j < Q_COUNT - 1 ? questState(j) : 0;
        boxText(2 + r, 3, (u8 *)qName[j], done ? FONT_GOLD : FONT_WHITE);
        if (j == 2 && !done && tradeItem >= 1 && tradeItem <= 3)
            boxTextRight(2 + r, 29, (u8 *)tradeName[tradeItem], FONT_WHITE);
        else if (qOf)
        {
            c = qOf >= 10 ? 27 : 28; /* the slash, just before the total */
            boxNum(2 + r, 29, qOf);
            boxPut(2 + r, c, FONT_WHITE + '/' - 32);
            boxNum(2 + r, c - 1, qHave);
        }
        else if (done)
            boxPut(2 + r, 29, FONT_GOLD + '*' - 32);
    }
    boxText(6, 2, qCur == Q_COUNT - 1 ? (u8 *)"A: READ   B: BACK" : (u8 *)"B: BACK", FONT_GOLD);
    boxState = BOX_QUEST;
}

void openQuests(void)
{
    qCur = 0;
    qTop = 0;
    drawQuests();
}

static void listMove(u16 pressed, u8 n, void (*draw)(void))
{
    if ((pressed & KEY_UP) && qCur > 0)
    {
        qCur--;
        if (qCur < qTop) qTop = qCur;
    }
    else if ((pressed & KEY_DOWN) && qCur + 1 < n)
    {
        qCur++;
        if (qCur >= qTop + 4) qTop = qCur - 3;
    }
    else if ((pressed & KEY_LEFT) && qCur > 0)
    {
        qCur = qCur > 4 ? qCur - 4 : 0;
        qTop = qTop > 4 ? qTop - 4 : 0;
        if (qCur < qTop) qTop = qCur;
    }
    else if ((pressed & KEY_RIGHT) && qCur + 1 < n)
    {
        qCur = qCur + 4 < n ? qCur + 4 : n - 1;
        if (qCur >= qTop + 4) qTop = qCur - 3;
    }
    else
        return;
    sfx(SFX_MENU);
    draw();
}

void updateQuests(u16 pressed)
{
    if ((pressed & KEY_A) && qCur == Q_COUNT - 1)
    {
        cardReturn = 3;
        openTextAt((u8 *)&jour_txt + goalTxt[goalStep()]);
    }
    else if (pressed & (KEY_B | KEY_START))
        closeText();
    else
        listMove(pressed, Q_COUNT, drawQuests);
}

/* ---------- the journal: every story you've found, to read again ---------- */
static u8 loreHas(u8 k)
{
    return (loreRead[k >> 3] >> (k & 7)) & 1;
}

void drawJournal(void)
{
    u8 r, j, n = 0;
    boxDrawFrame();
    for (j = 0; j < LORE_COUNT; j++)
        if (loreHas(j)) n++;
    boxText(1, 2, (u8 *)"JOURNAL", FONT_GOLD);
    boxNum(1, 29, LORE_COUNT);
    boxPut(1, 27, FONT_GOLD + '/' - 32);
    boxNum(1, 26, n);
    for (r = 0; r < 4; r++)
    {
        j = qTop + r;
        if (j >= LORE_COUNT) break;
        boxPut(2 + r, 2, j == qCur ? FONT_GOLD + '>' - 32 : FILL);
        boxText(2 + r, 3, loreHas(j) ? (u8 *)loreTitle[j] : (u8 *)"- - - - - -", loreHas(j) ? FONT_WHITE : FONT_GOLD);
    }
    boxText(6, 2, (u8 *)"A: READ   B: BACK", FONT_GOLD);
    boxState = BOX_JOUR;
}

void openJournal(void)
{
    qCur = 0;
    qTop = 0;
    drawJournal();
}

void updateJournal(u16 pressed)
{
    if (pressed & KEY_A)
    {
        if (!loreHas(qCur))
        {
            sfx(SFX_FAIL);
            return;
        }
        cardReturn = 2;
        openTextAt((u8 *)&jour_txt + loreStory[qCur]);
    }
    else if (pressed & (KEY_B | KEY_START))
        closeText();
    else
        listMove(pressed, LORE_COUNT, drawJournal);
}

/* ---------- the bus map (web busTo): every neighbourhood you've been to is a ride away ---------- */
static u8 busCur;

static u8 owSeen(u8 k)
{
    return (roomFlags[owMap[k]] & ROOM_SEEN) != 0;
}

void drawBus(void)
{
    u8 k, r, c, here = owIdx[curMap], goal = goalOwMap();
    u16 f;
    boxDrawFrame();
    for (k = 0; k < OW_COUNT; k++)
    {
        r = 1 + owRow[k];
        c = 4 + owCol[k] * 4;
        if (k == busCur) boxPut(r, c - 1, FONT_GOLD + '>' - 32);
        f = k == here ? FONT_GOLD : FONT_WHITE;
        if (owSeen(k) || k == here) boxText(r, c, (u8 *)owCode[k], f);
        else boxText(r, c, (u8 *)"...", FONT_WHITE);
        if (k == goal && (frameCount & 16)) boxPut(r, c + 3, FONT_GOLD + '!' - 32);
    }
    boxText(6, 2, owSeen(busCur) || busCur == here ? (u8 *)owName[busCur] : (u8 *)"???", FONT_GOLD);
    boxState = BOX_BUS;
}

void openBus(void)
{
    busCur = owIdx[curMap];
    if (busCur == 255) busCur = 0;
    drawBus();
}

/* the nearest map in a direction on the grid */
static void busStep(s8 dc, s8 dr)
{
    u8 k, best = 255, bd = 255, d;
    s8 c0 = owCol[busCur], r0 = owRow[busCur], dc2, dr2;
    for (k = 0; k < OW_COUNT; k++)
    {
        dc2 = (s8)owCol[k] - c0;
        dr2 = (s8)owRow[k] - r0;
        if (dc && (dc2 * dc <= 0 || dr2 != 0)) continue;
        if (dr && (dr2 * dr <= 0 || dc2 != 0)) continue;
        d = (u8)(dc2 < 0 ? -dc2 : dc2) + (u8)(dr2 < 0 ? -dr2 : dr2);
        if (d < bd) bd = d, best = k;
    }
    if (best == 255) return;
    busCur = best;
    sfx(SFX_MENU);
    drawBus();
}

void updateBus(u16 pressed)
{
    if (pressed & KEY_LEFT) busStep(-1, 0);
    else if (pressed & KEY_RIGHT) busStep(1, 0);
    else if (pressed & KEY_UP) busStep(0, -1);
    else if (pressed & KEY_DOWN) busStep(0, 1);
    else if (pressed & KEY_A)
    {
        if (busCur == owIdx[curMap])
        {
            sfx(SFX_FAIL);
            return;
        }
        if (!owSeen(busCur))
        {
            cardReturn = 4;
            openTextAt((u8 *)&jour_txt + TXJ_BUSNEW);
            return;
        }
        closeText();
        sfx(SFX_JING);
        px = owArrX[busCur] * 16 + 8;
        py = owArrY[busCur] * 16 + 8;
        heroDir = 0;
        enterMap(owMap[busCur]);
    }
    else if (pressed & (KEY_B | KEY_START))
        closeText();
    else if ((frameCount & 15) == 0)
        drawBus(); /* the goal marker blinks */
}
