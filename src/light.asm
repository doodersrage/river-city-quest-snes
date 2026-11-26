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

.RAMSECTION ".lightram" BANK $7E SLOT RAMSLOT_0
lightTab1 DSB 451
lightTab2 DSB 451
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
    beq +
    lda #451
+:
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

    ; every line empty
    ldy #112
    lda #$00FF
-:
    sta.l lightTab1+1,x
    sta.l lightTab1+226,x
    inx
    inx
    dey
    bne -

    ; table 1 keeps the HUD rows (0-23) lit
    lda 19
    bne +
    ldx #0
    lda #$FF00
--:
    sta.l lightTab1+1,x
    inx
    inx
    cpx #48
    bne --
+:

    lda 25
    beq _done
    eor #$FFFF
    inc a
    sta 3                   ; dy = -r

_line:
    lda 23
    clc
    adc 3
    bmi _next               ; above the screen
    cmp #224
    bcs _next               ; below it
    sta 5                   ; line
    lda 19
    bne +
    lda 5
    cmp #24
    bcc _next               ; the HUD rows are already lit
+:
    lda 3
    bpl +
    eor #$FFFF
    inc a
+:
    tay
    lda [15],y
    and #$00FF
    sta 7                   ; w = hw[|dy|]

    lda 21
    clc
    adc 7
    bmi _next               ; right edge off the left of the screen
    cmp #256
    bcc +
    lda #255
+:
    tay                     ; Y = right
    lda 21
    sec
    sbc 7
    bpl +
    lda #0                  ; left edge clipped to the screen
+:
    cmp #256
    bcs _next               ; left edge off the right of the screen
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
    bcc +
    inx                     ; the second block has its own header byte
+:
    pla
    sta.l lightTab1,x

_next:
    lda 3
    inc a
    sta 3
    sec
    sbc 25
    bmi _line
    beq _line

_done:
    tsc
    clc
    adc #8
    tcs
    pld
    plp
    rtl

.ENDS
