/* Day and night, and dark rooms (stage 8; web systems.js dark(), drawNight, draw.js drawLight).

   A day is 36000 frames (ten minutes), starting at 9 in the morning. Outdoors it gets dark
   from 5 to 8 in the evening and light again from 5 to 7 in the morning; rooms the web game
   doesn't light (dungeons, caves, museums) are always dark. Darkness is colour math: the
   screen's BG1, sprites in palettes 4-7 and the backdrop get a fixed colour subtracted (and
   halved at full dark), except inside two windows, each a circle of light from an HDMA table
   built by lightCircle (lightcircle.asm): one around the hero, bigger with the Gas Lantern, and one
   on the nearest lit brazier, wall torch, street lamp or lit window. Text (BG3) and the
   hero and townsfolk (sprite palettes 0-3) stay bright, as do the HUD rows. */
#include "game.h"

extern u8 lightTab1[], lightTab2[];
void lightCircle(const u8 *hw, u16 which, s16 cx, s16 cy, u16 r);
extern u8 *brazPtr;
extern u8 roomBraz[];
extern u8 hasLantern;

#define DAY 36000
#define HOUR 1500

#define REG_WOBJSEL (*(vuint8 *)0x2125)
#define REG_WOBJLOG (*(vuint8 *)0x212B)
#define REG_TMW (*(vuint8 *)0x212E)
#define REG_TSW (*(vuint8 *)0x212F)
#define REG_CGWSEL (*(vuint8 *)0x2130)
#define REG_CGADSUB (*(vuint8 *)0x2131)
#define REG_COLDATA (*(vuint8 *)0x2132)
#define REG_DMAP(c) (*(vuint8 *)(0x4300 + (c) * 16))
#define REG_BBAD(c) (*(vuint8 *)(0x4301 + (c) * 16))
#define REG_A1TL(c) (*(vuint8 *)(0x4302 + (c) * 16))
#define REG_A1TH(c) (*(vuint8 *)(0x4303 + (c) * 16))
#define REG_A1B(c) (*(vuint8 *)(0x4304 + (c) * 16))

u16 dayClock;
u8 darkLevel;           /* 0 light .. 4 full dark */
u8 lightR1, lightR2;    /* radii drawn (0 none) */
s16 lightX1, lightY1, lightX2, lightY2;
u8 lightDirty;

static const u8 *hwFor(u8 r)
{
    switch (r)
    {
    case 15: return hw15;
    case 32: return hw32;
    case 38: return hw38;
    case 44: return hw44;
    case 46: return hw46;
    case 48: return hw48;
    case 54: return hw54;
    default: return hw68;
    }
}

void lightInit(void)
{
    u16 a;
    dayClock = 4500; /* 9 in the morning */
    darkLevel = 0;
    lightR1 = lightR2 = 255;
    lightDirty = 1;
    /* HDMA 4 -> WH0/WH1 (window 1), HDMA 5 -> WH2/WH3 (window 2), two registers a line
       (PVSnesLib's vblank sends the sprites over DMA channel 7, so not that one) */
    a = (u16)lightTab1;
    REG_DMAP(4) = 0x01;
    REG_BBAD(4) = 0x26;
    REG_A1TL(4) = a & 255;
    REG_A1TH(4) = a >> 8;
    REG_A1B(4) = 0x7E;
    a = (u16)lightTab2;
    REG_DMAP(5) = 0x01;
    REG_BBAD(5) = 0x28;
    REG_A1TL(5) = a & 255;
    REG_A1TH(5) = a >> 8;
    REG_A1B(5) = 0x7E;
    lightCircle(hw15, 0, 0, 0, 0);
    lightCircle(hw15, 1, 0, 0, 0);
}

/* web dark(): 1 from 8pm to 5am, ramping over 5-8pm and 5-7am; here in steps 0..4 */
static u8 nightLevel(void)
{
    u16 c = dayClock;
    if (c >= 14 * HOUR && c < 23 * HOUR) return 4;          /* 8pm - 5am */
    if (c >= 11 * HOUR && c < 14 * HOUR) return (c - 11 * HOUR) / (HOUR * 3 / 4); /* 5pm - 8pm */
    if (c >= 23 * HOUR) return 4 - (c - 23 * HOUR) / (HOUR / 2); /* 5am - 6am */
    if (c < HOUR) return 2 - c / (HOUR / 2);                  /* 6am - 7am */
    return 0;
}

/* once a game frame: the clock, how dark it is, and where the light falls */
void lightFrame(void)
{
    u8 kind = mapDark[curMap], r1, r2, k, n, lit;
    u16 i, best, d;
    s16 hx, hy, x, y, bx, by;
    u8 *b;
    if (boxState != BOX_MENU && ++dayClock >= DAY) dayClock = 0;
    darkLevel = kind == 1 ? 0 : kind >= 2 ? 4 : nightLevel();
    r1 = r2 = 0;
    hx = px - camX;
    hy = py - 4 - camY;
    bx = by = 0;
    if (darkLevel)
    {
        r1 = kind >= 2 ? (hasLantern ? 68 : 38) : (hasLantern ? 54 : 32);
        /* the nearest light on screen: lit braziers, then the map's torches, lamps and lit windows */
        best = 0xFFFF;
        if (kind >= 2 && brazPtr)
            for (k = 0; k < brazPtr[0]; k++)
                if (roomBraz[curMap] & (1 << k))
                {
                    b = brazPtr + 1 + k * 10;
                    x = b[0] * 16 + 8 - camX;
                    y = b[1] * 16 + 6 - camY;
                    d = ABS(x - hx) + ABS(y - hy);
                    if (d < best)
                    {
                        best = d;
                        bx = x;
                        by = y;
                        r2 = 44;
                    }
                }
        for (i = lightOff[curMap]; i < lightOff[curMap + 1]; i++)
        {
            b = (u8 *)lightData + i * 3;
            x = (b[0] << 1) - camX;
            y = (b[1] << 1) - camY;
            if (x < -48 || x > 304 || y < -48 || y > 272) continue;
            d = ABS(x - hx) + ABS(y - hy);
            if (d < best)
            {
                best = d;
                bx = x;
                by = y;
                r2 = b[2] == 0 ? 46 : b[2] == 1 ? 48 : 15;
            }
        }
    }
    if (r1 != lightR1 || hx != lightX1 || hy != lightY1)
    {
        lightR1 = r1;
        lightX1 = hx;
        lightY1 = hy;
        lightCircle(hwFor(r1), 0, hx, hy, r1);
    }
    if (r2 != lightR2 || bx != lightX2 || by != lightY2)
    {
        lightR2 = r2;
        lightX2 = bx;
        lightY2 = by;
        lightCircle(hwFor(r2), 1, bx, by, r2);
    }
    lightDirty = 1;
}

/* in vertical blank: the colour math registers for this frame */
void lightVBlank(void)
{
    u8 kind;
    if (!lightDirty) return;
    lightDirty = 0;
    if (!darkLevel)
    {
        REG_CGADSUB = 0;
        REG_HDMAEN = 0;
        return;
    }
    kind = mapDark[curMap];
    REG_WOBJSEL = 0xA0; /* colour window = window 1 and window 2 */
    REG_WOBJLOG = 0;    /* OR */
    REG_TMW = 0;
    REG_TSW = 0;
    REG_CGWSEL = 0x20;  /* no colour math inside the colour window; math against the fixed colour */
    if (darkLevel >= 4)
    {
        /* subtract, halve: BG1, sprites (palettes 4-7) and the backdrop */
        REG_CGADSUB = 0x80 | 0x40 | 0x01 | 0x10 | 0x20;
        if (kind == 3)
        { /* the tunnel: web rgba(20,4,24,.72), a purple dark */
            REG_COLDATA = 0x20 | 2;
            REG_COLDATA = 0x40 | 5;
            REG_COLDATA = 0x80 | 1;
        }
        else
        { /* web rgba(8,6,24,.68) / the night's navy */
            REG_COLDATA = 0x20 | 4;
            REG_COLDATA = 0x40 | 3;
            REG_COLDATA = 0x80 | 0;
        }
    }
    else
    {
        /* dusk and dawn: subtract a little, bluer as it gets darker */
        REG_CGADSUB = 0x80 | 0x01 | 0x10 | 0x20;
        REG_COLDATA = 0x20 | (darkLevel * 3);
        REG_COLDATA = 0x40 | (darkLevel * 3);
        REG_COLDATA = 0x80 | darkLevel;
    }
    REG_HDMAEN = 0x30;
}
