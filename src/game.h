/* Shared state between main.c (world, text) and combat.c (fighting). */
#ifndef GAME_H
#define GAME_H
#include <snes.h>
#include "maps.h"

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
#define MAX_EN 6
#define MAX_PK 6

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
extern u8 *brazPtr;
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

#endif
