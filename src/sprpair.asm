;---------------------------------------------------------------------------------
; A 16x32 character in one call (stage 12).
;
; The hero and the townspeople are two 16x16 sprites each, and while the camera scrolls every one of them moves every
; frame. Going through oamSet from C (eight arguments a call) twice per character cost too much of the frame, so
; this writes both OAM entries directly: the top half at y-16 with tile, the bottom at y with tile+2, both large,
; same attributes. It marks both slots shown in oamShown (main.c), so sprHide still works on them.
;
; void sprPair(u8 top, u8 bot, s16 x, s16 y, u16 tile, u8 attr)
;   attr: the OAM attribute byte without the tile's top bit: vhoopppc, i.e. (hflip << 6) | (prio << 4) | (pal << 1)
;---------------------------------------------------------------------------------

.include "hdr.asm"

.BASE $00
.RAMSECTION ".sprpairram" BANK $7E SLOT 2
sprMsb DSB 2    ; 4 when x is 256 or more (the ninth bit of x): picks the second half of the mask tables
sprDY  DSB 2    ; this half's y offset: -16 for the top
sprDT  DSB 2    ; and tile offset: +2 for the bottom
.ENDS

.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".sprpair_text" SUPERFREE

; per slot within its high-table byte: clear its two bits, then set size=large and the x ninth bit
sprHiClr:
    .db $fc, $f3, $cf, $3f, $fc, $f3, $cf, $3f
sprHiSet:
    .db $02, $08, $20, $80, $03, $0c, $30, $c0

sprPair:
    php
    phb
    phx
    phy
    sep #$20
    lda #$7E
    pha
    plb                     ; oamMemory is in bank $7E
    rep #$30
    ; args now: top 10,s  bot 11,s  x 12,s  y 14,s  tile 16,s  attr 18,s
    lda 12,s
    and.w #$0100
    beq +
    lda.w #4
+   sta.l sprMsb
    lda.w #$FFF0            ; the top half: 16 px up, the same tile
    sta.l sprDY
    lda.w #0
    sta.l sprDT
    lda 10,s
    and.w #$00FF
    jsr sprOne
    lda.w #0                ; the bottom half: at y, two tiles on
    sta.l sprDY
    lda.w #2
    sta.l sprDT
    lda 11,s
    and.w #$00FF
    jsr sprOne
    ply
    plx
    plb
    plp
    rtl

; A (16-bit) = slot. Called with 16-bit A/X/Y; args are 2 further down the stack (the jsr), then 2 more after pha.
sprOne:
    pha                     ; args: x 16,s  y 18,s  tile 20,s  attr 22,s
    asl a
    asl a
    tax                     ; X = slot * 4
    sep #$20
    lda 16,s                ; x, low byte
    sta.w oamMemory,x
    rep #$20
    lda 18,s                ; y
    clc
    adc.l sprDY
    sep #$20
    sta.w oamMemory+1,x
    rep #$20
    lda 20,s                ; tile
    clc
    adc.l sprDT
    sep #$20
    sta.w oamMemory+2,x
    xba                     ; the tile's high byte: its bit 0 is the attribute's name-table bit
    and #$01
    ora 22,s
    sta.w oamMemory+3,x
    rep #$20
    pla                     ; the slot again
    tax
    sep #$20
    lda #2
    sta.l oamShown,x        ; shown, large (main.c's bookkeeping)
    rep #$20
    txa
    lsr a
    lsr a
    clc
    adc.w #512
    tay                     ; Y = its byte in the high table
    txa
    and.w #$0003
    clc
    adc.l sprMsb
    tax                     ; X = its place in the mask tables
    sep #$20
    lda.w oamMemory,y
    and.l sprHiClr,x
    ora.l sprHiSet,x
    sta.w oamMemory,y
    rep #$20
    rts

.ENDS
