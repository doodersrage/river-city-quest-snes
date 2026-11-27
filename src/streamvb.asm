;---------------------------------------------------------------------------------
; Stage 13: the map at double size, streamed (the C half is src/stream.c; this file can't be stream.asm, which is what compiling stream.c makes).
;
; The tables live in bank $7F. Each VBlank, streamVBlank copies the queued tiles (32 bytes each, from the map's tile
; set in ROM) into their VRAM slots, then writes the queued tilemap words, then sets BG1's scroll, so a frame never
; shows a new scroll without its new edge.
;---------------------------------------------------------------------------------

.include "hdr.asm"

.define UPQ 72      ; tiles a VBlank (a column and a row of new tiles at worst: 29 + 33)
.define WRQ 72      ; tilemap words a VBlank

.BASE $00
.RAMSECTION ".streamram" BANK $7F SLOT 3
sGrid    DSB 64*32*2     ; the metatile of each 32 px cell (patched as chests open, bushes go...)
sSlotOf  DSB 2048*2      ; the VRAM slot holding each of the map's tiles (0: none)
sRef     DSB 1024*2      ; how many loaded cells use each slot ($4000 and up: pinned, the animated tiles)
sFree    DSB 1024*2      ; free slots, a stack
upAddr   DSB UPQ*2       ; tile uploads: source address in its bank,
upBank   DSB UPQ         ; the bank,
upDst    DSB UPQ*2       ; and the VRAM word address
wrAddr   DSB WRQ*2       ; tilemap writes: VRAM word address
wrVal    DSB WRQ*2       ; and the word
upN      DSB 2
wrN      DSB 2
tsAddr   DSB 4           ; the tile set's two 32 KB chunks: address in bank
tsBank   DSB 2           ; and bank
scrX     DSB 2           ; BG1 scroll to set in the next VBlank
scrY     DSB 2
.ENDS

.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".stream_text" SUPERFREE

;---------------------------------------------------------------------------------
; void streamSetTs(u8 k, u8 *chunk)  -- where chunk k (0 or 1) of the tile set is
streamSetTs:
    php
    rep #$30
    lda 5,s                 ; k (a byte; the high byte is the pointer's low byte, masked off)
    and.w #$00FF
    asl a
    tax
    lda 6,s                 ; the chunk's address in its bank
    sta.l tsAddr,x
    txa
    lsr a
    tax
    sep #$20
    lda 8,s                 ; its bank
    sta.l tsBank,x
    plp
    rtl

;---------------------------------------------------------------------------------
; void streamVBlank(void)  -- from rcqVBlank, right after WaitForVBlank
streamVBlank:
    php
    phb
    sep #$20
    lda #$00
    pha
    plb                     ; registers in bank 0
    rep #$30
    ; ---- tiles: one 32-byte DMA each, on channel 0 ----
    lda.l upN
    beq stTilesDone
    asl a
    sta.l upN               ; (now the end, counted in words)
    ldx.w #0
    sep #$20
    lda #$80
    sta.w $2115             ; VRAM: increment after the high byte
    lda #$01
    sta.w $4300             ; two registers, $2118/$2119
    lda #$18
    sta.w $4301
    rep #$20
stTile:
    lda.l upDst,x
    sta.w $2116
    lda.l upAddr,x
    sta.w $4302
    lda.w #32
    sta.w $4305
    txa
    lsr a
    tay                     ; Y = entry number, for the byte-wide bank table
    phx
    tyx
    sep #$20
    lda.l upBank,x
    sta.w $4304
    lda #$01
    sta.w $420B             ; go
    rep #$20
    plx
    inx
    inx
    txa
    cmp.l upN
    bcc stTile
    lda.w #0
    sta.l upN
stTilesDone:
    ; ---- tilemap words ----
    lda.l wrN
    beq stWordsDone
    asl a
    sta.l wrN
    ldx.w #0
    sep #$20
    lda #$80
    sta.w $2115
    rep #$20
stWord:
    lda.l wrAddr,x
    sta.w $2116
    lda.l wrVal,x
    sta.w $2118
    inx
    inx
    txa
    cmp.l wrN
    bcc stWord
    lda.w #0
    sta.l wrN
stWordsDone:
    ; ---- BG1 scroll (each register takes the low byte, then the high) ----
    sep #$20
    lda.l scrX
    sta.w $210D
    lda.l scrX+1
    sta.w $210D
    lda.l scrY
    sta.w $210E
    lda.l scrY+1
    sta.w $210E
    plb
    plp
    rtl

.ENDS
