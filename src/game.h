/* Shared state between main.c (world, text) and combat.c (fighting). */
#ifndef GAME_H
#define GAME_H
#include <snes.h>
#include "maps.h"

#define MAP_PX_W (MAP_W * 16)
#define MAP_PX_H (MAP_H * 16)
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
#define MAX_EN 6
#define MAX_PK 6

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

u8 solidAt(s16 x, s16 y);
u8 blocked(s16 x, s16 y);
void enterMap(u8 i);
void openTextAt(u8 *p);

void combatInit(void);
void combatLoad(u8 *ene);
void combatUpdate(u16 pad, u16 pressed);
void combatDraw(void);
void hudDraw(void);
u8 heroBusy(void);
u16 rnd(void);

#endif
