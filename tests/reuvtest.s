; Does a 17xx REU read the pair after a verify difference?
;

CHROUT  = $ffd2
PTR     = $fb                   ; a spare zero page pointer
CASE    = $fd                   ; and a second one
SDR     = $dc0c
ICR     = $dc0d
PALNTSC = $02a6                 ; the KERNAL's PAL flag
VBUF    = $c000                 ; the RAM the second table verifies

        .segment "LOADADDR"
        .word $0801

        .segment "CODE"
        .word @eol, 2026
        .byte $9e, "2061", 0    ; SYS 2061
@eol:   .word 0

start:  lda #<msg_title
        ldy #>msg_title
        jsr prstr

        ;; An REU's registers read back what was written; open bus does not.
        lda #$a5
        sta $df02
        lda #$5a
        sta $df03
        lda $df02
        cmp #$a5
        bne noreu
        lda $df03
        cmp #$5a
        beq reu
noreu:  lda #<msg_noreu
        ldy #>msg_noreu
        jmp prstr

reu:    sei
        lda #$7f                ; no interrupts from CIA 1 while it is used
        sta ICR
        lda #0                  ; timer A stopped
        sta $dc0e

        ;; The REU bytes the verifies compare with: the first differs from
        ;; $DC0C, which is the same every time it is read.
        lda SDR
        eor #$ff
        sta buf
        lda #<buf
        sta $df02
        lda #>buf
        sta $df03
        lda #0
        sta $df04
        sta $df05
        sta $df06
        sta $df08
        sta $df09
        sta $df0a
        lda #4
        sta $df07
        lda #$90                ; execute now, C64 to REU
        sta $df01

        lda #<cases
        sta CASE
        lda #>cases
        sta CASE+1
next:   ldy #0
        lda (CASE),y
        iny
        ora (CASE),y
        beq ramtests
        jsr runcase
        lda CASE
        clc
        adc #6
        sta CASE
        bcc next
        inc CASE+1
        bne next

ramtests:
        lda #<msg_ram
        ldy #>msg_ram
        jsr prstr
        lda #$11
        sta VBUF
        lda #$22
        sta VBUF+1
        lda #$33
        sta VBUF+2
        lda #<ramcases
        sta CASE
        lda #>ramcases
        sta CASE+1
@next:  ldy #0
        lda (CASE),y
        iny
        ora (CASE),y
        beq done
        jsr ramcase
        lda CASE
        clc
        adc #4
        sta CASE
        bcc @next
        inc CASE+1
        bne @next

        ;; Back to how the KERNAL keeps CIA 1: timer A continuous at the
        ;; jiffy rate, its interrupt the only one enabled.
done:   ldx #$25
        ldy #$40
        lda PALNTSC
        beq @ntsc
        bne @set
@ntsc:  ldx #$95
        ldy #$42
@set:   stx $dc04
        sty $dc05
        lda #$11
        sta $dc0e
        lda ICR
        lda #$81
        sta ICR
        cli
        rts

; One line of the table: name, C64 address, length; no transfer for 0.
runcase:
        ldy #0
        lda (CASE),y
        tax
        iny
        lda (CASE),y
        tay
        txa
        jsr prstr

        ;; Arm the flag: timer A once, from 8, with its interrupt masked.
        lda ICR                 ; nothing left over
        lda #8
        sta $dc04
        lda #0
        sta $dc05
        lda #$19                ; one-shot, load, start
        sta $dc0e
        ldx #40
@wait:  dex
        bne @wait

        ldy #4
        lda (CASE),y
        iny
        ora (CASE),y
        beq @look
        lda $df00               ; status cleared
        ldy #2
        lda (CASE),y
        sta $df02
        iny
        lda (CASE),y
        sta $df03
        iny
        lda (CASE),y
        sta $df07
        iny
        lda (CASE),y
        sta $df08
        lda #0
        sta $df04
        sta $df05
        sta $df06
        sta $df09
        sta $df0a
        lda #$93                ; execute now, verify
        sta $df01
        lda $df00
        sta status

@look:  lda ICR
        and #1
        bne @kept
        lda #<msg_read
        ldy #>msg_read
        bne @say
@kept:  lda #<msg_kept
        ldy #>msg_kept
@say:   jsr prstr

        ldy #4
        lda (CASE),y
        iny
        ora (CASE),y
        beq eol
result: lda status
        jsr prhex
        lda #' '
        jsr CHROUT
        lda $df03
        jsr prhex
        lda $df02
        jsr prhex
        lda #' '
        jsr CHROUT
        lda $df08
        jsr prhex
        lda $df07
        jsr prhex
eol:    lda #13
        jmp CHROUT

; One line of the RAM table: name, the REU bytes to verify $C000 against.
ramcase:
        ldy #0
        lda (CASE),y
        tax
        iny
        lda (CASE),y
        tay
        txa
        jsr prstr

        ldy #2                  ; the REU bytes to REU $000100
        lda (CASE),y
        sta $df02
        iny
        lda (CASE),y
        sta $df03
        jsr @regs
        lda #$90                ; execute now, C64 to REU
        sta $df01

        lda $df00               ; status cleared
        lda #<VBUF
        sta $df02
        lda #>VBUF
        sta $df03
        jsr @regs
        lda #$93                ; execute now, verify
        sta $df01
        lda $df00
        sta status

        and #$40
        beq @noeob
        lda #<msg_eob
        ldy #>msg_eob
        bne @eob
@noeob: lda #<msg_none
        ldy #>msg_none
@eob:   jsr prstr
        lda status
        and #$20
        beq @noflt
        lda #<msg_flt
        ldy #>msg_flt
        bne @flt
@noflt: lda #<msg_none
        ldy #>msg_none
@flt:   jsr prstr
        jmp result

        ;; REU $000100, three bytes, both addresses counting.
@regs:  lda #0
        sta $df04
        sta $df06
        sta $df08
        sta $df09
        sta $df0a
        lda #1
        sta $df05
        lda #3
        sta $df07
        rts

prhex:  pha
        lsr
        lsr
        lsr
        lsr
        jsr @nib
        pla
        and #$0f
@nib:   cmp #10
        bcc @digit
        adc #$40-10             ; carry set: 10 is $41, a
        bne @out
@digit: ora #$30
@out:   jmp CHROUT

prstr:  sta PTR
        sty PTR+1
        ldy #0
@loop:  lda (PTR),y
        beq @done
        jsr CHROUT
        iny
        bne @loop
@done:  rts

        .segment "RODATA"
cases:  .word name0, 0, 0
        .word name1, $dc0d, 1
        .word name2, $dc0c, 1
        .word name3, $dc0c, 2
        .word name4, $dc0c, 3
        .word 0

name0:  .byte "no transfer     ", 0
name1:  .byte "dc0d alone      ", 0
name2:  .byte "diff on last    ", 0
name3:  .byte "diff 2nd last   ", 0
name4:  .byte "diff mid-block  ", 0

ramcases:
        .word rname0, pat0
        .word rname1, pat1
        .word rname2, pat2
        .word rname3, pat3
        .word rname4, pat4
        .word 0

rname0: .byte "all equal       ", 0
rname1: .byte "diff last only  ", 0
rname2: .byte "diff 2nd last   ", 0
rname3: .byte "diff both last  ", 0
rname4: .byte "diff mid-block  ", 0

pat0:   .byte $11, $22, $33
pat1:   .byte $11, $22, $cc
pat2:   .byte $11, $dd, $33
pat3:   .byte $11, $dd, $cc
pat4:   .byte $ee, $22, $33

msg_title:  .byte 147, "is addr after vfy fail read? (cia 1)", 13, 13
            .byte "                dc0d st addr len", 13, 0
msg_ram:    .byte 13, "verify of c000-c002 in ram:", 13
            .byte "                eob flt st addr len", 13, 0
msg_eob:    .byte "eob ", 0
msg_flt:    .byte "flt ", 0
msg_none:   .byte "    ", 0
msg_read:   .byte "read ", 0
msg_kept:   .byte "kept ", 0
msg_noreu:  .byte "no reu at $df00", 13, 0

        .segment "BSS"
buf:    .res 4
status: .res 1
