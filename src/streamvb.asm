;---------------------------------------------------------------------------------
; Stage 13: the map at double size, streamed (the C half is src/stream.c; this file can't be stream.asm, which is what compiling stream.c makes).
;
; The tables live in bank $7F. Each VBlank, streamVBlank copies the queued tiles (32 bytes each, from the map's tile
; set in ROM) into their VRAM slots, then writes the queued tilemap words, then sets BG1's scroll, so a frame never
; shows a new scroll without its new edge.
;---------------------------------------------------------------------------------

.include "hdr.asm"

.define UPQ 96      ; tiles a VBlank (a column and a row of new tiles at worst: 29 + 33, and a changed cell)
.define WRQ 96      ; tilemap words a VBlank

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
; stLine's state (kept here, all reached with long addressing while DB holds the metatiles' bank)
stTX     DSB 2
stTY     DSB 2
stDX     DSB 2
stDY     DSB 2
stN      DSB 2
stPut    DSB 2
stW      DSB 2
stID     DSB 2
stSlot   DSB 2
stTmp    DSB 2
stTmp2   DSB 2
stMetaAddr DSB 2         ; the metatile table: address in its bank
stMetaBank DSB 2         ; and the bank
stTW     DSB 2           ; the map's size in tiles
stTH     DSB 2
stFreeTop DSB 2
gridRow  DSB 64*2        ; byte offset of each row of cells in sGrid
.ENDS

.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".stream_text" SUPERFREE

;---------------------------------------------------------------------------------
; void streamReset(u8 *grid, u16 cells, u16 s0)
; A new map: copy its grid into sGrid, forget every cached tile (sSlotOf, sRef) and fill the free stack with
; slots s0-1 .. 1 (slot 0 is the blank tile, s0.. the pinned animated tiles). In C this took a dozen frames.
streamReset:
    php
    phb
    rep #$30
    lda 6,s
    tax                     ; X = the grid's address
    lda 10,s
    asl a
    sta.l stN               ; bytes to copy
    sep #$20
    lda 8,s
    pha
    plb                     ; DB = the grid's bank
    rep #$20
    ldy.w #0
srGrid:
    tya
    cmp.l stN
    bcs srClear
    lda.w $0000,x
    phx
    tyx
    sta.l sGrid,x
    plx
    inx
    inx
    iny
    iny
    bra srGrid
srClear:
    lda.w #0
    ldx.w #0
srSlot:
    sta.l sSlotOf,x
    inx
    inx
    cpx.w #2048*2
    bne srSlot
    ldx.w #0
srRef:
    sta.l sRef,x
    inx
    inx
    cpx.w #1024*2
    bne srRef
    ; the free stack: sFree[i] = s0 - 1 - i
    lda 12,s
    dec a
    sta.l stFreeTop
    ldx.w #0
srFree:
    cmp.w #0
    beq srDone
    sta.l sFree,x
    inx
    inx
    dec a
    bra srFree
srDone:
    plb
    plp
    rtl

;---------------------------------------------------------------------------------
; void streamSetMeta(u8 *meta)  -- where the metatile table is
streamSetMeta:
    php
    rep #$20
    lda 5,s
    sta.l stMetaAddr
    lda 7,s
    and.w #$00FF
    sta.l stMetaBank
    plp
    rtl

;---------------------------------------------------------------------------------
; void stLine(u16 tx, u16 ty, u16 dx, u16 dy, u16 n, u8 put)
; n tiles from (tx, ty), stepping (dx, dy): put = 1 loads each (caches its tile, queues its tilemap word), 0 drops
; it (one use less of its slot; a slot nobody uses goes back on the free stack). Tiles outside the map load as 0.
stLine:
    php
    phb
    rep #$30
    lda 6,s
    sta.l stTX
    lda 8,s
    sta.l stTY
    lda 10,s
    sta.l stDX
    lda 12,s
    sta.l stDY
    lda 14,s
    sta.l stN
    lda 16,s
    and.w #$00FF
    sta.l stPut
    sep #$20
    lda.l stMetaBank
    pha
    plb                     ; DB = the metatiles' bank
    rep #$20
stLoop:
    lda.l stN
    bne stGo
    jmp stEnd
stGo:
    dec a
    sta.l stN
    lda.l stTX
    cmp.l stTW
    bcc stInX
    jmp stOut
stInX:
    lda.l stTY
    cmp.l stTH
    bcc stIn
    jmp stOut
stIn:
    ; the cell's metatile: sGrid[gridRow[ty >> 2] + (tx >> 2) * 2]
    lda.l stTY
    lsr a
    lsr a
    asl a
    tax
    lda.l gridRow,x
    sta.l stTmp
    lda.l stTX
    lsr a
    lsr a
    asl a
    clc
    adc.l stTmp
    tax
    lda.l sGrid,x
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.l stTmp             ; metatile * 32
    lda.l stTY
    and.w #3
    asl a
    asl a
    sta.l stTmp2
    lda.l stTX
    and.w #3
    clc
    adc.l stTmp2
    asl a
    clc
    adc.l stTmp
    clc
    adc.l stMetaAddr
    tay
    lda.w $0000,y           ; the map word
    sta.l stW
    and.w #$07FF
    sta.l stID
    asl a
    tax                     ; X = tile * 2
    lda.l stPut
    bne stPutOne
    jmp stDropOne
stPutOne:
    lda.l sSlotOf,x
    bne stHave
    lda.l stFreeTop
    bne stAlloc
    jmp stOut               ; (no slot left: draw the blank tile)
stAlloc:
    dec a
    sta.l stFreeTop
    phx
    asl a
    tax
    lda.l sFree,x
    plx
    sta.l sSlotOf,x
    sta.l stSlot
    jsr stQueueUp
    lda.l stSlot
stHave:
    sta.l stSlot
    asl a
    tax
    lda.l sRef,x
    inc a
    sta.l sRef,x
    lda.l stW
    lsr a
    and.w #$1C00
    ora.l stSlot
    sta.l stTmp
    lda.l stW
    and.w #$C000
    ora.l stTmp
    bra stWrite
stOut:
    lda.l stPut
    beq stNext
    lda.w #0
stWrite:
    sta.l stTmp
    jsr stQueueWr
stNext:
    lda.l stTX
    clc
    adc.l stDX
    sta.l stTX
    lda.l stTY
    clc
    adc.l stDY
    sta.l stTY
    jmp stLoop
stDropOne:
    lda.l sSlotOf,x
    beq stNext
    sta.l stSlot
    phx
    asl a
    tax
    lda.l sRef,x
    dec a
    sta.l sRef,x
    bne stKeep
    plx
    lda.w #0
    sta.l sSlotOf,x
    lda.l stFreeTop
    asl a
    tax
    lda.l stSlot
    sta.l sFree,x
    lda.l stFreeTop
    inc a
    sta.l stFreeTop
    bra stNext
stKeep:
    plx
    bra stNext
stEnd:
    plb
    plp
    rtl

; queue the upload of tile stID into slot stSlot
stQueueUp:
    lda.l upN
    cmp.w #UPQ
    bcs stQUFull
    asl a
    tax
    lda.l stID
    and.w #$03FF
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.l stTmp2
    lda.l stID
    and.w #$0400
    beq stQU0
    lda.l tsAddr+2
    clc
    adc.l stTmp2
    sta.l upAddr,x
    phx
    txa
    lsr a
    tax
    sep #$20
    lda.l tsBank+1
    sta.l upBank,x
    rep #$20
    plx
    bra stQUDst
stQU0:
    lda.l tsAddr
    clc
    adc.l stTmp2
    sta.l upAddr,x
    phx
    txa
    lsr a
    tax
    sep #$20
    lda.l tsBank
    sta.l upBank,x
    rep #$20
    plx
stQUDst:
    lda.l stSlot
    asl a
    asl a
    asl a
    asl a
    sta.l upDst,x
    lda.l upN
    inc a
    sta.l upN
stQUFull:
    rts

; queue the tilemap word stTmp for tile (stTX, stTY): its place in the 64x64 ring at $4000
stQueueWr:
    lda.l wrN
    cmp.w #WRQ
    bcs stQWFull
    asl a
    tax
    lda.l stTY
    and.w #32
    asl a
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.l stTmp2
    lda.l stTX
    and.w #32
    asl a
    asl a
    asl a
    asl a
    asl a
    clc
    adc.l stTmp2
    sta.l stTmp2
    lda.l stTY
    and.w #31
    asl a
    asl a
    asl a
    asl a
    asl a
    clc
    adc.l stTmp2
    sta.l stTmp2
    lda.l stTX
    and.w #31
    clc
    adc.l stTmp2
    clc
    adc.w #$4000
    sta.l wrAddr,x
    lda.l stTmp
    sta.l wrVal,x
    lda.l wrN
    inc a
    sta.l wrN
stQWFull:
    rts

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
