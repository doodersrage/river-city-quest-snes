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
stMetaAddr DSB 2         ; the metatile table: address in its bank
stMetaBank DSB 2         ; and the bank
stTW     DSB 2           ; the map's size in tiles
stTH     DSB 2
stFreeTop DSB 2
stN      DSB 2           ; (streamReset's byte count)
gridRow  DSB 64*2        ; byte offset of each row of cells in sGrid
.ENDS

.RAMSECTION ".streamdp" BANK 0 SLOT 1
stDP     DSB 40          ; stLine's working state, on the direct page while it runs (see D_* below)
.ENDS

.ENUM 0
D_TX     DW
D_TY     DW
D_DX     DW
D_DY     DW
D_N      DW
D_PUT    DW
D_W      DW
D_ID     DW
D_SLOT   DW
D_T      DW
D_T2     DW
D_MA     DW
D_TW     DW
D_TH     DW
D_FT     DW
D_UN     DW
D_WN     DW
.ENDE

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
; Runs with D on stDP (bank 0 RAM) and DB on the metatiles' bank: the hot state is all direct page.
stLine:
    php
    phb
    phd
    rep #$30
    lda.w #stDP
    tcd
    lda 8,s
    sta.b D_TX
    lda 10,s
    sta.b D_TY
    lda 12,s
    sta.b D_DX
    lda 14,s
    sta.b D_DY
    lda 16,s
    sta.b D_N
    lda 18,s
    and.w #$00FF
    sta.b D_PUT
    lda.l stMetaAddr
    sta.b D_MA
    lda.l stTW
    sta.b D_TW
    lda.l stTH
    sta.b D_TH
    lda.l stFreeTop
    sta.b D_FT
    lda.l upN
    sta.b D_UN
    lda.l wrN
    sta.b D_WN
    sep #$20
    lda.l stMetaBank
    pha
    plb                     ; DB = the metatiles' bank
    rep #$20
stLoop:
    lda.b D_N
    bne stGo
    jmp stEnd
stGo:
    dec a
    sta.b D_N
    lda.b D_TX
    cmp.b D_TW
    bcs stOutFar
    lda.b D_TY
    cmp.b D_TH
    bcc stIn
stOutFar:
    jmp stOut
stIn:
    ; the cell's metatile: sGrid[gridRow[ty >> 2] + (tx >> 2) * 2]
    lsr a
    lsr a
    asl a
    tax
    lda.l gridRow,x
    sta.b D_T
    lda.b D_TX
    lsr a
    lsr a
    asl a
    clc
    adc.b D_T
    tax
    lda.l sGrid,x
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.b D_T               ; metatile * 32
    lda.b D_TY
    and.w #3
    asl a
    asl a
    sta.b D_T2
    lda.b D_TX
    and.w #3
    adc.b D_T2              ; (carry clear: the asl's shifted out zeros)
    asl a
    adc.b D_T
    clc
    adc.b D_MA
    tay
    lda.w $0000,y           ; the map word
    sta.b D_W
    and.w #$07FF
    sta.b D_ID
    asl a
    tax                     ; X = tile * 2
    lda.b D_PUT
    bne stPutOne
    jmp stDropOne
stPutOne:
    lda.l sSlotOf,x
    bne stHave
    lda.b D_FT
    beq stOut               ; (no slot left: draw the blank tile)
    dec a
    sta.b D_FT
    asl a
    phx
    tax
    lda.l sFree,x           ; sFree[top]
    plx
    sta.l sSlotOf,x
    sta.b D_SLOT
    jsr stQueueUp
    lda.b D_SLOT
stHave:
    sta.b D_SLOT
    asl a
    tax
    lda.l sRef,x
    inc a
    sta.l sRef,x
    lda.b D_W
    lsr a
    and.w #$1C00
    ora.b D_SLOT
    sta.b D_T
    lda.b D_W
    and.w #$C000
    ora.b D_T
    bra stWrite
stOut:
    lda.b D_PUT
    beq stNext
    lda.w #0
stWrite:
    sta.b D_T
    jsr stQueueWr
stNext:
    lda.b D_TX
    clc
    adc.b D_DX
    sta.b D_TX
    lda.b D_TY
    clc
    adc.b D_DY
    sta.b D_TY
    jmp stLoop
stDropOne:
    lda.l sSlotOf,x
    beq stNext
    sta.b D_SLOT
    txy                     ; Y = tile * 2
    asl a
    tax
    lda.l sRef,x
    dec a
    sta.l sRef,x
    bne stNext
    tyx
    lda.w #0
    sta.l sSlotOf,x         ; nobody uses the slot: back on the free stack
    lda.b D_FT
    asl a
    tax
    lda.b D_SLOT
    sta.l sFree,x
    inc.b D_FT
    bra stNext
stEnd:
    lda.b D_FT
    sta.l stFreeTop
    lda.b D_UN
    sta.l upN
    lda.b D_WN
    sta.l wrN
    pld
    plb
    plp
    rtl

; queue the upload of tile D_ID into slot D_SLOT
stQueueUp:
    lda.b D_UN
    cmp.w #UPQ
    bcs stQUFull
    asl a
    tax
    lda.b D_ID
    and.w #$03FF
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.b D_T2
    lda.b D_ID
    and.w #$0400
    beq stQU0
    lda.l tsAddr+2
    clc
    adc.b D_T2
    sta.l upAddr,x
    sep #$20
    lda.l tsBank+1
    bra stQUBank
stQU0:
    lda.l tsAddr
    clc
    adc.b D_T2
    sta.l upAddr,x
    sep #$20
    lda.l tsBank
stQUBank:
    pha
    rep #$20
    txa
    lsr a
    tax
    sep #$20
    pla
    sta.l upBank,x
    rep #$20
    txa
    asl a
    tax
    lda.b D_SLOT
    asl a
    asl a
    asl a
    asl a
    sta.l upDst,x
    inc.b D_UN
stQUFull:
    rts

; queue the tilemap word D_T for tile (D_TX, D_TY): its place in the 64x64 ring at $4000
stQueueWr:
    lda.b D_WN
    cmp.w #WRQ
    bcs stQWFull
    asl a
    tax
    lda.b D_TY
    and.w #32
    asl a
    sta.b D_T2              ; (ty & 32) << 1, shifted 5 more below with the row
    lda.b D_TY
    and.w #31
    ora.b D_T2
    asl a
    asl a
    asl a
    asl a
    asl a
    sta.b D_T2              ; ((ty & 31) << 5) + ((ty & 32) << 6)
    lda.b D_TX
    and.w #32
    asl a
    asl a
    asl a
    asl a
    asl a
    ora.b D_T2
    sta.b D_T2
    lda.b D_TX
    and.w #31
    ora.b D_T2
    ora.w #$4000
    sta.l wrAddr,x
    lda.b D_T
    sta.l wrVal,x
    inc.b D_WN
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
