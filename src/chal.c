/* River City Quest (SNES): the 14 neighbourhood challenges and the history quiz (web people.js offer, startChal,
   chalProgress, chalWin, chalFail, runQuiz). Win one for a Richmond city pin; all fourteen turn your paddle gold. */
#include <snes.h>
#include "game.h"

#define OAM_CHAL 120 /* 7: seconds left (2) and the count (up to 5) */
#define OAM_BANG 127 /* the "!" over someone with a challenge you haven't won */

u8 chalOn;      /* challenge index + 1, or 0 */
u8 chalGot, chalNeed, chalSpawned, chalBits;
u16 chalT;      /* frames left */
u16 pins;       /* a bit per challenge won */
u8 goldPaddle;
u8 loreRead[(LORE_COUNT + 7) / 8];

u8 askKind, askN, askCur, askChal;
const char *askOpt[3];

/* the quiz */
u8 quizI, quizScore, quizAns, quizQ[3];

static u8 hudSecs, hudGot, hudShown, bangK;
static u16 bangBit;
static s16 bangX, bangY;

void chalReset(void)
{
    hudShown = 1; /* hides the countdown sprites on the next draw */
    bangK = 255;
}

/* Called when a map loads: who here has a challenge (for the "!"). */
void chalLoad(void)
{
    u8 i;
    u8 *rec = wld + WLD_NPCS;
    bangK = 255;
    bangX = -100;
    sprHide(OAM_BANG);
    for (i = 0; i < npcCount; i++, rec += NPC_REC)
        if (rec[12])
        {
            bangK = i;
            bangBit = 1 << (rec[12] - 1);
        }
}

static void chalCells(u8 c, u8 i, s16 *x, s16 *y)
{
    u8 o = (chalOff[c] + i) << 1;
    *x = chalXY[o] * 16 + 8;
    *y = chalXY[o + 1] * 16 + 8;
}

u8 pinCount(void)
{
    u8 c, n = 0;
    for (c = 0; c < CHAL_COUNT; c++)
        if (pins & (1 << c)) n++;
    return n;
}

/* Reading a marker adds its story to your journal. */
void loreMark(u8 text)
{
    u16 i;
    for (i = 0; i < LORE_REFS; i++)
        if (loreRef[i * 3] == curMap && loreRef[i * 3 + 1] == text)
        {
            loreRead[loreRef[i * 3 + 2] >> 3] |= 1 << (loreRef[i * 3 + 2] & 7);
            return;
        }
}

static u8 loreKnown(u8 k)
{
    return (loreRead[k >> 3] >> (k & 7)) & 1;
}

/* Talking to someone with a challenge: offer it (the question comes when the text ends). */
void chalOffer(u8 c)
{
    openTextAt((u8 *)&sys_txt + ((pins & (1 << c)) ? chalAgainTxt[c] : chalOfferTxt[c]));
    askKind = ASK_CHAL;
    askChal = c;
    askN = 2;
    askCur = 0;
    askOpt[0] = "LET'S GO!";
    askOpt[1] = "NOT NOW";
}

static void clearChal(void)
{
    u8 k;
    for (k = 0; k < MAX_PK; k++)
        if (pkKind[k] == PK_TOKEN || pkKind[k] == PK_FLAG) pkKind[k] = 0;
    for (k = 0; k < MAX_EN; k++)
        if (enChal[k])
        {
            enAlive[k] = 0;
            enChal[k] = 0;
        }
    chalOn = 0;
}

static void spawnChalFoe(void)
{
    u8 c = chalOn - 1, k, nc = chalOff[c + 1] - chalOff[c];
    s16 x, y;
    for (k = 0; k < MAX_EN; k++)
        if (!enAlive[k])
        {
            chalCells(c, chalSpawned % nc, &x, &y);
            if (chalSpawned >= nc) x += 6;
            spawnEnemy(chalFoe[c], x, y);
            enChal[k] = 1;
            chalSpawned++;
            return;
        }
}

static void quizAsk(void)
{
    u8 i, j, o;
    quizAns = rnd() % 3;
    askOpt[quizAns] = loreTitle[quizQ[quizI]];
    for (i = 0; i < 3; i++)
    {
        if (i == quizAns) continue;
        do
        {
            o = rnd() % LORE_COUNT;
            for (j = 0; j < 3; j++)
                if (j != i && askOpt[j] == loreTitle[o]) break;
        } while (o == quizQ[quizI] || j < 3);
        askOpt[i] = loreTitle[o];
    }
    openTextAt((u8 *)&sys_txt + loreTxt[quizQ[quizI]]);
    askKind = ASK_QUIZ;
    askN = 3;
    askCur = 0;
}

static void quizStart(void)
{
    u8 k, n = 0, i, j;
    for (k = 0; k < LORE_COUNT; k++)
        n += loreKnown(k);
    if (n < 3)
    {
        openTextAt((u8 *)&sys_txt + TXT_QUIZFEW);
        return;
    }
    /* three different stories from your journal */
    for (i = 0; i < 3; i++)
    {
        do
        {
            k = rnd() % LORE_COUNT;
            for (j = 0; j < i; j++)
                if (quizQ[j] == k) break;
        } while (!loreKnown(k) || j < i);
        quizQ[i] = k;
    }
    quizI = 0;
    quizScore = 0;
    quizAsk();
}

static void chalWin(void)
{
    u8 c = chalOn - 1, first = !(pins & (1 << c));
    clearChal();
    pins |= 1 << c;
    coins += first ? 25 : 10;
    if (coins > 999) coins = 999;
    sfx(SFX_JING);
    if (!first)
    {
        openTextAt((u8 *)&sys_txt + TXT_PINAGAIN);
        return;
    }
    if (pinCount() == CHAL_COUNT) goldPaddle = 1;
    openTextAt((u8 *)&sys_txt + pinTxt[pinCount() - 1]);
}

static void chalStart(u8 c)
{
    u8 i, n;
    s16 x, y;
    if (chalNeeds[c] == 1 && !hasBug)
    {
        openTextAt((u8 *)&sys_txt + TXT_NEEDBUG);
        return;
    }
    if (chalNeeds[c] == 2 && !hasLantern)
    {
        openTextAt((u8 *)&sys_txt + TXT_NEEDLAMP);
        return;
    }
    if (chalType[c] == CH_QUIZ)
    {
        quizStart();
        return;
    }
    clearChal();
    chalOn = c + 1;
    chalT = chalSecs[c] * 60;
    chalGot = 0;
    chalBits = 0;
    chalSpawned = 0;
    n = chalOff[c + 1] - chalOff[c];
    chalNeed = n;
    if (chalType[c] == CH_COLLECT)
    {
        for (i = 0; i < MAX_PK; i++) pkKind[i] = 0; /* room for every token */
        for (i = 0; i < n; i++)
        {
            chalCells(c, i, &x, &y);
            dropPickup(x, y, PK_TOKEN, 255);
        }
    }
    else if (chalType[c] == CH_RACE)
    {
        chalCells(c, 0, &x, &y);
        dropPickup(x, y, PK_FLAG, 255);
        chalNeed = 1;
    }
    else if (chalType[c] == CH_DEFEAT)
    {
        chalNeed = chalN[c];
        for (i = 0; i < chalNeed; i++) spawnChalFoe();
    }
    sfx(SFX_GO);
    openTextAt((u8 *)&sys_txt + chalGoTxt[c]);
}

/* A on the question at the end of a text. */
/* Someone who runs a minigame offers it (their own words, then the question). */
void miniOffer(u8 k, u8 text)
{
    openText(text);
    askKind = ASK_MINI;
    askChal = k;
    askN = 2;
    askCur = 0;
    askOpt[0] = "LET'S PLAY!";
    askOpt[1] = "NOT NOW";
}

void askChoose(u8 i)
{
    closeText();
    if (askKind == ASK_MINI)
    {
        askKind = 0;
        if (i == 0) runMini(askChal);
        return;
    }
    if (askKind == ASK_CHAL)
    {
        askKind = 0;
        if (i == 0) chalStart(askChal);
        return;
    }
    askKind = 0;
    /* the quiz */
    if (i == quizAns)
    {
        quizScore++;
        sfx(SFX_CHAL);
    }
    else sfx(SFX_HURT);
    if (++quizI < 3)
    {
        quizAsk();
        return;
    }
    if (quizScore == 3)
    {
        chalOn = CHAL_QUIZ + 1;
        chalWin();
    }
    else openTextAt((u8 *)&sys_txt + (quizScore == 0 ? TXT_QUIZ0 : quizScore == 1 ? TXT_QUIZ1 : TXT_QUIZ2));
}

void chalProgress(void)
{
    if (!chalOn) return;
    chalGot++;
    sfx(SFX_CHAL);
    if (chalGot >= chalNeed) chalWin();
}

/* a challenge foe goes down */
void chalKill(u8 k)
{
    enChal[k] = 0;
    if (!chalOn) return;
    if (chalSpawned < chalNeed) spawnChalFoe();
    chalProgress();
}

/* The lantern lights a gas lamp, or A finds a mural; 1 if it counted. */
u8 chalAt(u8 type, s16 fx, s16 fy)
{
    u8 c = chalOn - 1, i, n;
    if (!chalOn || chalType[c] != type) return 0;
    n = chalOff[c + 1] - chalOff[c];
    for (i = 0; i < n; i++)
        if (chalXY[(chalOff[c] + i) << 1] == (fx >> 4) && chalXY[((chalOff[c] + i) << 1) + 1] == (fy >> 4))
        {
            if (chalBits & (1 << i)) return 0;
            chalBits |= 1 << i;
            chalProgress();
            return 1;
        }
    return 0;
}

void chalUpdate(void)
{
    u8 c;
    if (!chalOn) return;
    c = chalOn - 1;
    if (curMap != chalMap[c])
    {
        chalOn = 0; /* walked away: the challenge is off */
        return;
    }
    if (!--chalT)
    {
        clearChal();
        sfx(SFX_FAIL);
        openTextAt((u8 *)&sys_txt + chalFailTxt[c]);
    }
}

static void put8(u8 slot, s16 x, u16 tile)
{
    sprPut(slot, 0, x, 8, 3, 0, tile, PAL_HUD);
}

/* Seconds left and how many so far, top middle (web: name, seconds, got/need). */
void chalDraw(void)
{
    u8 s, i, x;
    s16 sx, sy;
    /* the "!" over the person with a challenge you haven't won, blinking */
    if (bangK != 255)
    {
        sx = npcX[bangK] - camX - 8;
        sy = npcY[bangK] - camY - 32;
        if (chalOn || boxState || ((frameCount >> 4) & 1) || (pins & bangBit) || sx < -16 || sx > SCREEN_W || sy < -16 || sy > SCREEN_H)
        {
            sprHide(OAM_BANG);
            bangX = -100;
        }
        else if (sx != bangX || sy != bangY)
        {
            sprPut(OAM_BANG, 1, sx, sy, 3, 0, townTile[TOWN_BANG], PAL_ITEM);
            bangX = sx;
            bangY = sy;
        }
    }
    if (!chalOn)
    {
        if (hudShown)
        {
            for (i = 0; i < 7; i++) sprHide(OAM_CHAL + i);
            hudShown = 0;
        }
        return;
    }
    s = (chalT + 59) / 60;
    if (hudShown && s == hudSecs && chalGot == hudGot) return;
    hudShown = 1;
    hudSecs = s;
    hudGot = chalGot;
    /* 00 seconds, a gap, then got/need */
    put8(OAM_CHAL, 120, HUD_DIGIT + s / 10); /* clear of ten hearts */
    put8(OAM_CHAL + 1, 128, HUD_DIGIT + s % 10);
    x = 148;
    i = OAM_CHAL + 2;
    if (chalGot >= 10)
    {
        put8(i++, x, HUD_DIGIT + 1);
        x += 8;
    }
    put8(i++, x, HUD_DIGIT + chalGot % 10);
    x += 8;
    put8(i++, x, HUD_SLASH);
    x += 8;
    if (chalNeed >= 10)
    {
        put8(i++, x, HUD_DIGIT + 1);
        x += 8;
    }
    put8(i++, x, HUD_DIGIT + chalNeed % 10);
    while (i < OAM_CHAL + 7) sprHide(i++);
}
