;---------------------------------------------------------------------------------
; Button taps that land in a lag frame (stage 10).
;
; PVSnesLib only reads the joypad at the end of a frame the game finished in time, so a tap
; shorter than a long frame could be missed. This VBlank routine (set with nmiSet) runs on
; every VBlank and ORs the auto-read joypad 1 bits into padSeen; the main loop takes it and
; clears it once a frame.
;---------------------------------------------------------------------------------

.include "hdr.asm"

.BASE $00
.RAMSECTION ".padram" BANK $7E SLOT 2
padSeen DSB 2
.ENDS

.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".padnmi_text" SUPERFREE

; void rcqNmi(void): called from the VBlank ISR with 16-bit A/X/Y
rcqNmi:
    php
    jsl consoleVblank           ; the library's own VBlank work
    sep #$20
rcqNmiWait:
    lda.l $004212               ; wait for the joypad auto-read to finish
    and #$01
    bne rcqNmiWait
    rep #$20
    lda.l $004218
    ora.l padSeen
    sta.l padSeen
    plp
    rtl

.ENDS
