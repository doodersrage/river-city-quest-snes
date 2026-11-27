;---------------------------------------------------------------------------------
; Collision against the map's cells, in assembly (stage 13e). The C versions were the biggest cost in a frame:
; every enemy step tests four corners, and tcc makes each test a multiply, signed shifts and a run of compares.
;
;   u8 solidAt(s16 x, s16 y)    a wall at map pixel (x, y)? (off the map: no, that's an exit; the river: unless
;                               you have the kayak)
;   u8 riverAt(s16 x, s16 y)    the river at (x, y)?
;   u8 enBlocked(s16 x, s16 y)  can't an enemy stand at (x, y)? (its box is 5 px each way; off the map, the river
;                               and walls all stop it)
;---------------------------------------------------------------------------------

.include "hdr.asm"

.define CELL_RIVER 10

.BASE $00
.RAMSECTION ".collide_ram" BANK $7F SLOT 3
ckTmp    DSB 2
ebX0     DSB 2
ebX1     DSB 2
ebY0     DSB 2
ebY1     DSB 2
.ENDS


.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".collide_text" SUPERFREE

; which cell kinds are walls (open, solid, door, exit, counter, bush, post, low, lock, brazier, river, sprout, water)
solidKinds:
    .db 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 0, 0

; in: A = x, X = y (16 bit, map pixels, both inside the map). out: A = the cell's kind (8 bit value, 16 bit A).
; Uses DB (sets it to colGrid's bank; callers save it).
cellKind:
    lsr a
    lsr a
    lsr a
    lsr a
    sta.l ckTmp             ; tx
    txa
    and.w #$FFF0
    asl a                   ; ty * 32 = (y >> 4) << 5 = (y & ~15) << 1
    clc
    adc.l ckTmp
    clc
    adc.l colGrid           ; + the grid's address
    tax
    lda.l colGrid+1
    xba
    sep #$20
    pha
    plb                     ; DB = the grid's bank
    lda.w $0000,x
    rep #$20
    and.w #$00FF
    rts

; is (A, X) on the map? carry clear if so (A, X unchanged)
onMap:
    cmp.w #0
    bmi omNo
    pha
    lsr a
    lsr a
    lsr a
    lsr a
    sep #$20
    cmp.l mapW
    rep #$20
    bcs omNo1
    txa
    bmi omNo1
    lsr a
    lsr a
    lsr a
    lsr a
    sep #$20
    cmp.l mapH
    rep #$20
    bcs omNo1
    pla
    clc
    rts
omNo1:
    pla
omNo:
    sec
    rts

;---------------------------------------------------------------------------------
solidAt:
    php
    phb
    rep #$30
    lda 8,s                 ; y
    tax
    lda 6,s                 ; x
    jsr onMap
    bcs saNo
    jsr cellKind
    cmp.w #CELL_RIVER
    bne saKind
    sep #$20
    lda.l hasKayak
    rep #$20
    and.w #$00FF
    beq saYes
    bra saNo
saKind:
    tax
    lda.l solidKinds,x
    and.w #$00FF
    bne saYes
saNo:
    lda.w #0
    bra saOut
saYes:
    lda.w #1
saOut:
    sta.l tcc__r0
    plb
    plp
    rtl

;---------------------------------------------------------------------------------
riverAt:
    php
    phb
    rep #$30
    lda 8,s                 ; y
    tax
    lda 6,s                 ; x
    jsr onMap
    bcs raNo
    jsr cellKind
    cmp.w #CELL_RIVER
    bne raNo
    lda.w #1
    bra raOut
raNo:
    lda.w #0
raOut:
    sta.l tcc__r0
    plb
    plp
    rtl

;---------------------------------------------------------------------------------
; enBlocked: each corner (x +- 5, y +- 5) must be on the map, not the river and not a wall
enBlocked:
    php
    phb
    rep #$30
    lda 6,s
    sec
    sbc.w #5
    sta.l ebX0
    clc
    adc.w #10
    sta.l ebX1
    lda 8,s
    sec
    sbc.w #5
    sta.l ebY0
    clc
    adc.w #10
    sta.l ebY1
    lda.l ebX0
    ldx.w #0
    jsr ebCorner
    bcs ebYes
    lda.l ebX1
    ldx.w #0
    jsr ebCorner
    bcs ebYes
    lda.l ebX0
    ldx.w #2
    jsr ebCorner
    bcs ebYes
    lda.l ebX1
    ldx.w #2
    jsr ebCorner
    bcs ebYes
    lda.w #0
    bra ebOut
ebYes:
    lda.w #1
ebOut:
    sta.l tcc__r0
    plb
    plp
    rtl

; A = x, X = 0 for ebY0 / 2 for ebY1; carry set if blocked
ebCorner:
    pha
    lda.l ebY0,x
    tax
    pla
    jsr onMap
    bcs ebcYes              ; off the map
    jsr cellKind
    cmp.w #CELL_RIVER
    beq ebcYes
    tax
    lda.l solidKinds,x
    and.w #$00FF
    bne ebcYes
    clc
    rts
ebcYes:
    sec
    rts

.ENDS

