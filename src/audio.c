/* Music and sound effects through SNESMOD (stage 7). The modules are generated from the web
   game's own songs by tools/mkaudio.py; module 0 holds the sound effects. */
#define RCQ_AUDIO_C
#include "game.h"
#include "../data/audio/soundbank.h"

extern char SOUNDBANK__;

u8 curSong;

void audioInit(void)
{
    spcBoot();
    spcSetBank(&SOUNDBANK__);
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
    playSong(bossOn ? SONG_BOSS : mapSong[curMap]);
}

void sfx(u8 id)
{
    if (curSong) spcEffect(sfxPitch[id], id, (sfxVol[id] << 4) | 8);
}

void rcqVBlank(void)
{
    WaitForVBlank();
    lightVBlank();
    spcProcess();
}
