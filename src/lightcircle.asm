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
; An empty line is left 255, right 0.
;---------------------------------------------------------------------------------

.include "hdr.asm"

.RAMSECTION ".lightram" BANK $7E SLOT 2
lightTab1 DSB 451
lightTab2 DSB 451
lightPrev DSB 8       ; cy, r of the circle on each table
.ENDS

.SECTION ".light_text" SUPERFREE

;---------------------------------------------------------------------------------
; void lightCircle(u8 *hw, u16 which, s16 cx, s16 cy, u16 r)
;   hw: half-widths of the circle, hw[0..r]; which: 0 table 1, 1 table 2;
;   cx, cy: screen centre; r: radius (0 = no circle)
; Stack after php, phd and 8 bytes of locals, with the direct page set to the stack:
;   1 base  3 dy  5 line  7 w   15 hw (24-bit)  19 which  21 cx  23 cy  25 r
lightCircle:
    php
    phd
    rep #$30
    tsc
    sec
    sbc #8
    tcs
    tcd

    ; base offset of this table
    lda 19
    beq lcP1
    lda #451
lcP1:
    sta 1

    ; headers
    tax
    sep #$20
    lda #$F0
    sta.l lightTab1,x
    sta.l lightTab1+225,x
    lda #0
    sta.l lightTab1+450,x
    rep #$20

    ; empty the lines the previous circle on this table covered (lightPrev: cy, r per table)
    lda 19
    asl a
    asl a
    tax
    lda.l lightPrev,x       ; previous cy
    sec
    sbc.l lightPrev+2,x     ; - previous r
    bpl lcC0
    lda #0
lcC0:
    sta 5                   ; first line to clear
    lda.l lightPrev,x
    clc
    adc.l lightPrev+2,x
    bpl lcC4
    lda #0                  ; all of it above the screen: nothing to clear
    sta 5
    inc 5
    bra lcC1
lcC4:
    cmp #224
    bcc lcC1
    lda #223
lcC1:
    sta 3                   ; last line to clear
    lda 23
    sta.l lightPrev,x       ; remember this circle for next time
    lda 25
    sta.l lightPrev+2,x

lcCLoop:
    lda 5
    cmp 3
    beq lcCLine
    bcs lcCDone
lcCLine:
    asl a
    clc
    adc 1
    inc a
    tax                     ; base + 1 + 2 * line
    lda 5
    cmp #112
    bcc lcC2
    inx
lcC2:
    lda #$00FF              ; empty: left 255, right 0
    ldy 19
    bne lcC3
    ldy 5
    cpy #24
    bcs lcC3
    lda #$FF00              ; table 1's HUD rows stay lit: left 0, right 255
lcC3:
    sta.l lightTab1,x
    inc 5
    bra lcCLoop
lcCDone:

    lda 25
    beq lcDone
    eor #$FFFF
    inc a
    sta 3                   ; dy = -r

lcLine:
    lda 23
    clc
    adc 3
    bmi lcNext               ; above the screen
    cmp #224
    bcs lcNext               ; below it
    sta 5                   ; line
    lda 19
    bne lcP5
    lda 5
    cmp #24
    bcc lcNext               ; the HUD rows are already lit
lcP5:
    lda 3
    bpl lcP6
    eor #$FFFF
    inc a
lcP6:
    tay
    lda [15],y
    and #$00FF
    sta 7                   ; w = hw[|dy|]

    lda 21
    clc
    adc 7
    bmi lcNext               ; right edge off the left of the screen
    cmp #256
    bcc lcP7
    lda #255
lcP7:
    tay                     ; Y = right
    lda 21
    sec
    sbc 7
    bpl lcP8
    lda #0                  ; left edge clipped to the screen
lcP8:
    cmp #256
    bcs lcNext               ; left edge off the right of the screen
    sta 7                   ; left
    tya
    xba
    ora 7
    pha                     ; left | right << 8

    lda 5
    asl a
    clc
    adc 1
    inc a
    tax                     ; base + 1 + 2 * line
    lda 5
    cmp #112
    bcc lcP9
    inx                     ; the second block has its own header byte
lcP9:
    pla
    sta.l lightTab1,x

lcNext:
    lda 3
    inc a
    sta 3
    sec
    sbc 25
    bmi lcLine
    beq lcLine

lcDone:
    tsc
    clc
    adc #8
    tcs
    pld
    plp
    rtl

.ENDS
