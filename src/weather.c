/* Weather (stage 8; web systems.js wxTick, drawSky, drawRain, drawSnow). Outdoors only.

   Every 50-120 seconds the sky may change: clear, rain, fog or snow. The web game weighs these
   by the real season; the SNES has no calendar, so this uses the year's average (clear half the
   time, rain 30%, fog 15%, snow 5%). Weather fades in over five seconds and out over four.
   Rain and snow are sprites falling across the screen; rain darkens the sky a little, fog and
   snow wash it out (colour math, unless night already uses it). */
#include "game.h"

#define WX_CLEAR 0
#define WX_RAIN 1
#define WX_FOG 2
#define WX_SNOW 3
#define DROPS 12
#define OAM_WX 100 /* 12 */

u8 wx, wxNext, wxK; /* weather, the one coming, strength 0..255 */
u16 wxT;
u8 dropX[DROPS], dropY[DROPS];
s16 wxCamX, wxCamY;
u8 wxShown;

void weatherInit(void)
{
    wx = WX_CLEAR;
    wxNext = 255;
    wxK = 0;
    wxT = 3000;
    wxShown = 1;
}

static u8 pickWeather(void)
{
    u16 r = rnd() % 100;
    return r < 50 ? WX_CLEAR : r < 80 ? WX_RAIN : r < 95 ? WX_FOG : WX_SNOW;
}

/* once a game frame (web wxTick) */
void weatherFrame(void)
{
    u8 i, n, outdoors = mapW == 32;
    s16 dx, dy;
    if (wxNext != 255)
    {
        if (wxK > 1) wxK -= 1;
        else
        {
            wxK = 0;
            wx = wxNext;
            wxNext = 255;
        }
    }
    else
    {
        if (wx != WX_CLEAR && wxK < 255) wxK++;
        if (--wxT == 0)
        {
            wxT = 3000 + rnd() % 4200;
            n = pickWeather();
            if (n != wx) wxNext = n;
        }
    }
    /* falling rain and snow: screen-space drops that keep their place in the world as the camera
       moves. Half of them move each frame, twice as far, to keep the frame rate. */
    n = outdoors && (wx == WX_RAIN || wx == WX_SNOW) ? (u8)((wxK >> 5) + 4) : 0;
    if (n > DROPS) n = DROPS;
    if (darkLevel) n >>= 1; /* at night the dark hides most of the sky: half as many */
    if (!n && !wxShown) return;
    dx = camX - wxCamX;
    dy = camY - wxCamY;
    wxCamX = camX;
    wxCamY = camY;
    for (i = 0; i < DROPS; i++)
    {
        if (i >= n || dying)
        {
            if (wxShown) sprHide(OAM_WX + i);
            continue;
        }
        dropX[i] -= dx;
        dropY[i] -= dy;
        if ((i & 1) != (frameCount & 1)) continue;
        if (wx == WX_RAIN)
        {
            dropX[i] -= 4;
            dropY[i] += 12;
        }
        else
        {
            /* snow drifts side to side and falls slowly */
            if (((frameCount >> 5) + i) & 2) dropX[i]++;
            else dropX[i]--;
            dropY[i] += 1;
        }
        if (dropY[i] > 224 || dropY[i] < 20)
        {
            dropY[i] = 24 + (rnd() & 15);
            dropX[i] = rnd();
        }
        sprPut(OAM_WX + i, 0, dropX[i], dropY[i], 3, 0, wx == WX_RAIN ? OBJ_RAIN : OBJ_SNOW, PAL_HUD);
    }
    wxShown = n != 0;
}

/* how the sky is tinted: 0 none, 1 rain (darker), 2 fog or snow (washed out); strength 0..3 */
u8 weatherTint(void)
{
    if (mapW != 32 || wx == WX_CLEAR || wxK < 80) return 0;
    return wx == WX_RAIN ? 1 : 2;
}
