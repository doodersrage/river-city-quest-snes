/* Shared state between main.c (world, text) and combat.c (fighting). */
#ifndef GAME_H
#define GAME_H
#include <snes.h>
#include "maps.h"
#include "../data/audio/sfx.h"

/* audio (audio.c): every frame wait also feeds the sound driver */
void audioInit(void);
void playSong(u8 song);
void songForMap(void);
void sfx(u8 id);
void rcqVBlank(void);

/* day, night and dark rooms (light.c) */
extern u16 dayClock;
extern u8 darkLevel;
void lightInit(void);
void lightFrame(void);
void lightVBlank(void);
u8 isNight(void);

/* weather (weather.c) */
extern u8 wx, wxK;
void weatherInit(void);
void weatherFrame(void);
u8 weatherTint(void);
extern u8 curSong;
#ifndef RCQ_AUDIO_C
#define WaitForVBlank() rcqVBlank()
#endif

extern u8 mapW, mapH; /* in 16px cells: 32x22 outdoors, 16x11 in rooms */
#define MAP_PX_W ((s16)mapW * 16)
#define MAP_PX_H ((s16)mapH * 16)
#define SCREEN_W 256
#define SCREEN_H 224
#define HALF 5 /* collision box half-size, as in the web game */
#define ABS(v) ((v) < 0 ? -(v) : (v))

/* OAM slots (lower slots draw on top). pvsneslib ids are slot * 4. */
#define OAM_HEARTS 0  /* 10 */
#define OAM_COIN 10
#define OAM_DIGITS 11 /* 3 */
#define OAM_HERO 14
#define OAM_PADDLE 15
#define OAM_NPCS 16   /* MAX_NPCS */
#define OAM_ENEMIES 20
#define OAM_PICKUPS 26
#define OAM_BOSS 40   /* 9 */
#define OAM_SHOTS 50  /* 10 */
#define OAM_HOOK 64   /* 14 */
#define OAM_BOMBS 80  /* 2 */
#define OAM_BOOM 82   /* 4 */
#define OAM_BICON 86  /* 3: item icon and count */
#define OAM_BUG 89
#define OAM_BLOCKS 90 /* 4 */
#define OAM_BALLS 94  /* 3 */
#define OAM_BIKE 97
#define MAX_EN 6
#define MAX_PK 10 /* room for a challenge's eight tokens */
#define PK_TOKEN 15
#define PK_FLAG 16
#define PK_FROG 17

/* VRAM (word addresses) */
#define VRAM_BG1_CHR 0x0000
#define VRAM_BG1_MAP 0x4000
#define VRAM_BG3_CHR 0x5000
#define VRAM_BG3_MAP 0x5800
#define VRAM_OBJ_CHR 0x6000

/* roomFlags bits */
#define ROOM_CLEARED 1
#define ROOM_BOSS 2
#define ROOM_CONTAINER 4
#define ROOM_SEEN 8
#define ROOM_SOLVED 16 /* clear rooms when cleared, lamp rooms when every brazier is lit */

extern u8 *colGrid;
extern u8 curMap;
extern s16 px, py, camX, camY;
extern u8 heroDir, step;
extern u16 frameCount;
extern u8 npcCount;
extern s16 npcX[], npcY[];
extern u8 npcDir[];
extern u8 *wld;

/* hero combat state */
extern u8 hp, hpMax, inv, atk, charging, charge, spin, kt;
extern s8 kx, ky;
extern u16 coins;
extern u8 dying;

extern u8 *chests;
extern u8 chestOpen[];
extern u8 roomArea, roomSolve, roomBoss, roomEntry;
extern u8 *dynPtr, *stairsPtr;
extern u8 roomFlags[];
extern u8 keys[];
extern u8 hasHook, hasPowder, hasLantern, hasShield, powder, eqItem, keystones, roomReward, dentMap, hookSt, bossOn;
extern u8 enemiesExisted;
extern u8 enCount, enAlive[];
extern u8 enChal[];
extern u8 pkKind[];
extern s16 pkX[], pkY[];
extern u16 pkLife[];
extern u8 pkHop[];
/* stage 10, part 3: fishing, frogs, landmark cards (fish.c) */
extern u8 hasRod, frogs, frogsDone, cardReturn;
extern u16 cards;
u8 nightLevel(void);
void fishCast(void);
void fishUpdate(u16 pressed);
void frogTick(void);
void gotFrog(void);
void gotPackCard(void);
u8 packSoldOut(void);
u8 cardCount(void);
void openCards(void);
void drawCards(void);
void updateCards(u16 pressed);
void fishReset(void);
extern u8 hasCamera;
void snapPhoto(void);
u8 photoCount(void);
void runAlbum(void);
/* stage 10: neighbourhood challenges and the history quiz (chal.c) */
#define ASK_CHAL 1
#define ASK_QUIZ 2
#define ASK_MINI 3
extern u8 miniOn, lightDirty;
void runMini(u8 k);
void miniVBlank(void);
extern u16 miniBest[];
void chalReset(void);
void miniOffer(u8 k, u8 text);
extern u8 askKind, askN, askCur, chalOn, goldPaddle;
extern const char *askOpt[];
extern u16 pins;
extern u8 loreRead[];
void chalOffer(u8 c);
void askChoose(u8 i);
void chalProgress(void);
void chalKill(u8 k);
u8 chalAt(u8 type, s16 fx, s16 fy);
void chalUpdate(void);
void chalDraw(void);
void chalLoad(void);
void loreMark(u8 text);
u8 pinCount(void);

u8 solidAt(s16 x, s16 y);
void sprHide(u8 slot);
void sprPut(u8 slot, u8 large, s16 x, s16 y, u8 prio, u8 flip, u16 tile, u8 pal);
u8 *chestRec(u8 k);
void queuePatch(u8 tx, u8 ty, u8 *w, u8 now);
void hurtHero(s16 sx, s16 sy);
void hurtHeroBy(s16 sx, s16 sy, u8 dmg);
void bombHitsEnemies(s16 x, s16 y);
void useItem(void);
void nextItem(void);
void gotItem(u8 item);
u8 hasItem(u8 item);
void spawnEnemy(u8 type, s16 x, s16 y);
u8 enemyCount(u8 type);
extern u8 *brazPtr, *pickPtr;
extern u8 pickupGot[];
extern u8 hasBug, hasCarkey, pieces, hasKayak, tradeItem, melonTaken;
extern u8 parts, bateauDone, hasLure;   /* stage 9: the bateau quest */
/* stage 9: town life (life.c) */
void lifeLoad(void);
void lifeUpdate(void);
void lifeDraw(void);
u8 carHit(s16 x, s16 y);
extern u8 *blkPtr;
u8 blockAtCell(s16 tx, s16 ty);
void tryPush(void);
void notPushing(void);
void gotPiece(void);
void dropPickup(s16 x, s16 y, u8 kind, u8 id);
u8 pickupAt(s16 x, s16 y);
void movePickup(u8 k, s16 x, s16 y);
void collectPickup(u8 k);
u8 bugStunsEnemy(s16 x, s16 y);
void gotKeystone(u8 id);
u8 enBlocked(s16 x, s16 y);
u8 enemiesAlive(void);
u8 hookHitsEnemy(s16 x, s16 y, s16 kx, s16 ky);
void hookGrabsPickup(s16 x, s16 y);
void dungeonInit(void);
void dungeonLoad(void);
void dungeonEntered(void);
void dungeonUpdate(void);
void dungeonDraw(void);
void applyRoomState(u8 now);
u8 tryUnlock(s16 x, s16 y);
u8 bossHit(u8 dmg, u8 src);
void fireHook(void);
u8 blocked(s16 x, s16 y);
void enterMap(u8 i);
void openTextAt(u8 *p);
void openText(u8 i);

void combatInit(void);
void combatLoad(u8 *ene);
void combatUpdate(u16 pad, u16 pressed);
void combatDraw(void);
void hudDraw(void);
u8 heroBusy(void);
void openShop(u8 id);
u16 rnd(void);
void dropKind(s16 x, s16 y, u8 kind);
void cutAt(s16 x, s16 y);

/* the text box (main.c) */
#define BOX_MENU 5
#define FILL (FONT_WHITE)
extern u8 boxState, boxTop;
void boxPut(u8 row, u8 col, u16 tile);
void boxDrawFrame(void);
void boxText(u8 row, u8 col, u8 *s, u16 font);
void boxTextRight(u8 row, u8 right, u8 *s, u16 font);
void boxNum(u8 row, u8 right, u16 v);
void closeText(void);
void checkExits(void);
void checkCells(void);
void settle(void);
extern u16 lastCell;
void hitEnemy(u8 k, u8 dmg, s16 kdx, s16 kdy);
extern s16 enX[], enY[];
extern s16 bossX, bossY;
extern const s8 dirX[4], dirY[4];

/* gear (gear.c) */
extern u8 hasBalls, hasShoes, hasBike, hasBottle, hasBib;
extern u8 bottleFull, balls, capBalls, capPowder, riding, dash;
extern u8 hudBalls, hudBottle;
void gearInit(void);
void useGear(void);
void gotGear(u8 item);
void updBalls(void);
void drawBalls(void);
void updDash(void);
void drawBike(void);
void hudCount(u8 n);
void openMenu(void);
void updateMenu(u16 pressed);

/* the ending (ending.c) */
#define BOX_ROWS_ALL (8 * 32)
extern u8 gameWon, endingPending;

/* stage 11: saves (save.c) and the title screen (title.c) */
extern u8 saveSlot;
extern char heroName[9];
extern u16 playMin;
void saveInit(void);
void saveGame(void);
void playTick(void);
u8 runTitle(void);
void openIntro(void);
void dungeonEntered(void);
void rcqNmi(void);
void runEnding(void);

#endif
