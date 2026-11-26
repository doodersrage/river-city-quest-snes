;---------------------------------------------------------------------------------
; Light circles for night and dark rooms (stage 8).
;
; Darkness is colour math (subtract a fixed colour) everywhere except inside the colour
; window, which is window 1 OR window 2. Each window's left/right edge comes from an HDMA
; table, one pair of bytes per scanline, so each window draws one circle of light:
; table 1 follows the hero (and keeps the HUD rows lit), table 2 the nearest lamp or brazier.
;
; Table layout (HDMA mode 1 to $2126/$2127 or $2128/$2129, repeat mode):
;   $F0, 112 x (left, right), $F0, 112 x (left, right), $00
; An empty line is left 255, right 0. Line n sits at base + 1 + 2n, plus one past line 111.
;---------------------------------------------------------------------------------

.include "hdr.asm"

; RAM lives in bank $7E whatever the ROM speed (FastROM moves code and data to banks $80 up)
.BASE $00
.RAMSECTION ".lightram" BANK $7E SLOT 2
lightTab1 DSB 451
lightTab2 DSB 451
lightPrev DSB 8       ; cy, r of the circle on each table
.ENDS

.ifdef FASTROM
.BASE $80
.else
.BASE $00
.endif

.SECTION ".light_text" SUPERFREE

;---------------------------------------------------------------------------------
; void lightCircle(u8 *hw, u16 which, s16 cx, s16 cy, u16 r)
;   hw: half-widths of the circle, hw[0..r]; which: 0 table 1, 1 table 2;
;   cx, cy: screen centre; r: radius (0 = no circle)
; With php, phd and 16 bytes of locals, and the direct page set to the stack:
;   locals  1 base  3 first  5 last  7 w  9 cx8  11 count  13 split  15 lo
;   args    23 hw (24-bit)  27 which  29 cx  31 cy  33 r
lightCircle:
    php
    phd
    rep #$30
    tsc
    sec
    sbc #16
    tcs
    tcd

    ; base offset of this table, where its second block starts, the first line it may change
    stz 15                  ; lo = 0
    lda 27
    beq lcT1
    lda #451
    bra lcT2
lcT1:
    lda #24                 ; table 1 keeps the HUD rows (0-23) lit
    sta 15
    lda #0
lcT2:
    sta 1
    clc
    adc #225
    sta 13                  ; split: X of line 112 before skipping its header byte

    ; headers
    ldx 1
    sep #$20
    lda #$F0
    sta.l lightTab1,x
    sta.l lightTab1+225,x
    lda #0
    sta.l lightTab1+450,x
    rep #$20

    ;----- empty the lines the previous circle on this table covered
    lda 27
    asl a
    asl a
    tax
    lda.l lightPrev,x
    sec
    sbc.l lightPrev+2,x
    cmp 15
    bpl lcC0
    lda 15                  ; first = max(cy - r, lo)
lcC0:
    sta 3
    lda.l lightPrev,x
    clc
    adc.l lightPrev+2,x
    cmp #224
    bmi lcC1
    lda #223                ; last = min(cy + r, 223)
lcC1:
    sta 5
    lda 31
    sta.l lightPrev,x       ; remember this circle for next time
    lda 33
    sta.l lightPrev+2,x

    lda 5
    sec
    sbc 3
    bmi lcCDone             ; nothing to clear
    inc a
    sta 11                  ; count
    jsr lcOffset            ; X = offset of line [3]
    lda #$00FF              ; empty: left 255, right 0
lcCLoop:
    sta.l lightTab1,x
    inx
    inx
    cpx 13
    bne lcC2
    inx
lcC2:
    dec 11
    bne lcCLoop
lcCDone:

    ;----- draw the new circle
    lda 33
    bne lcD0
    jmp lcDone
lcD0:
    lda 29
    cmp #256
    bcc lcD1
    jmp lcDone              ; centre off the left or right of the screen (rare): no circle
lcD1:
    sta 9                   ; cx8
    lda 31
    sec
    sbc 33
    cmp 15
    bpl lcD2
    lda 15
lcD2:
    sta 3                   ; first = max(cy - r, lo)
    lda 31
    clc
    adc 33
    cmp #224
    bmi lcD3
    lda #223
lcD3:
    sta 5                   ; last = min(cy + r, 223)
    cmp 3
    bpl lcD4
    jmp lcDone              ; off the screen
lcD4:
    jsr lcOffset            ; X = offset of the first line

    ; upper half: lines first .. min(cy - 1, last), hw index cy - line, falling
    lda 31
    sec
    sbc 3                   ; cy - first
    bmi lcLower             ; the circle starts at or below its centre
    beq lcLower
    tay
    lda 31
    dec a
    cmp 5
    bmi lcU0
    lda 5
lcU0:
    sec
    sbc 3
    inc a
    sta 11                  ; count
    sep #$20
lcULoop:
    lda [23],y
    sta 7
    clc
    adc 9
    bcc lcU1
    lda #$FF
lcU1:
    sta.l lightTab1+1,x     ; right = min(cx + w, 255)
    lda 9
    sec
    sbc 7
    bcs lcU2
    lda #0
lcU2:
    sta.l lightTab1,x       ; left = max(cx - w, 0)
    inx
    inx
    cpx 13
    bne lcU3
    inx
lcU3:
    dey
    dec 11
    bne lcULoop
    rep #$20

lcLower:
    ; lower half: lines max(cy, first) .. last, hw index line - cy, rising
    lda 31
    cmp 3
    bpl lcL0
    lda 3
lcL0:
    pha                     ; start line
    sec
    sbc 31
    tay                     ; hw index
    lda 5
    sec
    sbc 1,s
    bmi lcLSkip
    inc a
    sta 11                  ; count
    pla
    sep #$20
lcLLoop:
    lda [23],y
    sta 7
    clc
    adc 9
    bcc lcL1
    lda #$FF
lcL1:
    sta.l lightTab1+1,x
    lda 9
    sec
    sbc 7
    bcs lcL2
    lda #0
lcL2:
    sta.l lightTab1,x
    inx
    inx
    cpx 13
    bne lcL3
    inx
lcL3:
    iny
    dec 11
    bne lcLLoop
    rep #$20
    bra lcDone
lcLSkip:
    pla

lcDone:
    tsc
    clc
    adc #16
    tcs
    pld
    plp
    rtl

; X = offset in the tables of line [3]: base + 1 + 2 * line, plus one from line 112 on
lcOffset:
    lda 3
    asl a
    clc
    adc 1
    inc a
    tax
    lda 3
    cmp #112
    bcc lcO0
    inx
lcO0:
    rts

.ENDS
