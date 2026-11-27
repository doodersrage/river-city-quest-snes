/* Music and sound effects through SNESMOD (stage 7). The modules are generated from the web
   game's own songs by tools/mkaudio.py; module 0 holds the sound effects. */
#define RCQ_AUDIO_C
#include "game.h"
#include "../data/audio/soundbank.h"

/* Over 32 KB, smconv splits the bank (banks 52 and 53) and names the halves __0 and __1;
   SNESMOD wants them from the last one down. */
extern char SOUNDBANK__0, SOUNDBANK__1;

u8 curSong;

void audioInit(void)
{
    spcBoot();
    spcSetBank(&SOUNDBANK__1);
    spcSetBank(&SOUNDBANK__0);
    curSong = 0;
}

/* change song (blocks while the module uploads: do it with the screen off where we can) */
void playSong(u8 song)
{
    u8 i;
    if (song == curSong) return;
    curSong = song;
    spcStop();
    spcLoad(song);
    for (i = 0; i < SFX_COUNT; i++) spcLoadEffect(i);
    spcPlay(0);
}

/* the song this place plays: the boss theme while a boss is up */
void songForMap(void)
{
    /* web wantSong(): outdoors after dark it's the night theme */
    playSong(bossOn ? SONG_BOSS : mapDark[curMap] == 0 && isNight() ? SONG_NIGHT : mapSong[curMap]);
}

void sfx(u8 id)
{
    if (curSong) spcEffect(sfxPitch[id], id, (sfxVol[id] << 4) | 8);
}

void rcqVBlank(void)
{
    WaitForVBlank();
    if (sLive) streamVBlank(); /* the map's new edge and its scroll first */
    heroVBlank();
    paddleVBlank();
    bossBgVBlank();  /* a new frame of the hero, if he changed */
    npcVBlank();   /* and of one townsperson */
    animVBlank();  /* and some of the animated tiles */
    lightVBlank();
    textVBlank();  /* the window's gradient on or off */
    if (miniOn) miniVBlank(); /* its score line, while still in VBlank */
    spcProcess();
}
