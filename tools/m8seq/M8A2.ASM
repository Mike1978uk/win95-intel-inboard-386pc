; M8A2.COM - the accelerator rows chapter 8 leaves unmeasured (worklist A2), Mach8 8 bpp.
;
; Records raw pixels and status for each case so the card and a model run compare byte for byte;
; the probe does not judge. Cases:
;   1 hit test: CMD with DRAW=0, inside and outside the scissors - SUBSYS_STAT before/after, row 0
;   2 PATTERN_L/H: PIX_CNTL fixed-pattern fills of 64x4, two pattern pairs - rows 0-3, x 0-63
;   3 reads outside the scissors: scissors x 0-63, read row 0 (a ramp) across 256 pixels
;   4 blit source wrap: 8 pixels blitted from x=1020 (wraps past 1023?) to (0,20) - row 20
;   5 ERR_TERM -1 vs 0: a 20-pixel x-major line at y=40 / y=48, slope 1/4 - rows 40-51, x 0-31
;   6 DEST_CMP_FN with WRT_MASK 0Fh: ramp, compare code 20h (>=) colour 55h, fill EEh - row 0
; Writes M8A2.BIN: "M8A2", then each case's record in order. Engine only; DOS boot.

        org     100h

RES             equ     0800h

        jmp     start

oname   db      'M8A2.BIN', 0
m_ok    db      'M8A2: done, results in M8A2.BIN', 13, 10, '$'
m_out   db      'M8A2: cannot write M8A2.BIN', 13, 10, '$'
hdr     db      'M8A2'
recp    dw      0
pix     dw      0
row     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

; store the 16-bit SUBSYS_STAT
%macro  STAT 0
        mov     dx, 42E8h
        in      ax, dx
        mov     di, [recp]
        stosw
        mov     [recp], di
%endmacro

start:
        cld
        call    init
        mov     word [recp], RES

; ---- 1: hit test -------------------------------------------------------------------------
        call    ramp                    ; row 0 = 00..FF
        OUTW    0BEE8h, 1000h           ; scissors x 0-63, y 0-15
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 300Fh
        OUTW    0BEE8h, 403Fh
        OUTW    42E8h, 000Fh            ; clear the status bits
        STAT
        OUTW    0A6E8h, 0EEh            ; DRAW=0 fill across row 0, partly inside
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40E3h
        call    idle
        STAT
        OUTW    42E8h, 000Fh
        OUTW    86E8h, 0                ; DRAW=0 fill wholly outside (y=100)
        OUTW    82E8h, 100
        OUTW    9AE8h, 40E3h
        call    idle
        STAT
        call    scfull
        mov     word [row], 0
        call    readrec                 ; row 0 must still be the ramp

; ---- 2: PATTERN_L/H ----------------------------------------------------------------------
        mov     ax, 8005h               ; PATTERN_L 5, PATTERN_H Ah
        mov     bx, 900Ah
        call    pattern
        mov     ax, 8001h               ; PATTERN_L 1, PATTERN_H 8
        mov     bx, 9008h
        call    pattern

; ---- 3: reads outside the scissors -------------------------------------------------------
        call    ramp
        OUTW    0BEE8h, 403Fh           ; right scissor 63
        mov     word [row], 0
        call    readrec
        call    scfull

; ---- 4: blit source wrap -----------------------------------------------------------------
        call    clr0
        mov     word [pix], 0           ; x 1016-1023 = 10h-17h, x 0-7 = 20h-27h
.w1:    mov     ax, [pix]
        add     ax, 10h
        mov     bx, [pix]
        add     bx, 1016
        call    plot
        mov     ax, [pix]
        add     ax, 20h
        mov     bx, [pix]
        call    plot
        inc     word [pix]
        cmp     word [pix], 8
        jb      .w1
        call    idle
        OUTW    0BAE8h, 67h             ; source = screen, mix = new
        OUTW    0BEE8h, 0A000h
        OUTW    86E8h, 1020             ; source (1020,0)
        OUTW    82E8h, 0
        OUTW    8EE8h, 0                ; dest (0,20)
        OUTW    8AE8h, 20
        OUTW    96E8h, 7
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 0C0F3h
        call    idle
        OUTW    0BAE8h, 27h
        mov     word [row], 20
        call    readrec

; ---- 5: ERR_TERM -1 vs 0 -----------------------------------------------------------------
        call    clr40
        mov     ax, 0FFFFh
        mov     bx, 40
        call    line
        xor     ax, ax
        mov     bx, 48
        call    line
        mov     word [row], 40
.l5:    call    readrec
        inc     word [row]
        cmp     word [row], 52
        jb      .l5

; ---- 6: DEST_CMP_FN under WRT_MASK 0Fh ---------------------------------------------------
        call    ramp
        OUTW    0B2E8h, 55h             ; COLOR_CMP
        OUTW    0EEEEh, 20h             ; >= : keep where the destination is >= 55h
        OUTW    0AAE8h, 0Fh             ; WRT_MASK: low nibble only
        OUTW    0A6E8h, 0EEh
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0EEEEh, 0
        OUTW    0AAE8h, 0FFh
        mov     word [row], 0
        call    readrec

; ---- write the file ----------------------------------------------------------------------
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, [recp]
        sub     cx, RES
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; ---- helpers -----------------------------------------------------------------------------
init:
        OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        call    scfull
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        ret

scfull: OUTW    0BEE8h, 1000h           ; scissors 0-1023 both ways
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        ret

; one pixel: AL = colour, BX = x, row 0
plot:   push    ax
        call    idle
        pop     ax
        mov     dx, 0A6E8h
        out     dx, ax
        OUTW    0BAE8h, 27h
        mov     ax, bx
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 0
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        ret

; Row 0, x = 0..255, pixel x = x.
ramp:   OUTW    0EEEEh, 0
        mov     word [pix], 0
.p:     mov     ax, [pix]
        mov     bx, ax
        call    plot
        inc     word [pix]
        cmp     word [pix], 256
        jb      .p
        call    idle
        ret

; fill rows from (0, BX) 1024 wide, AX-1 rows, colour 0
fill0:  push    ax
        call    idle
        OUTW    0A6E8h, 0
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        mov     ax, bx
        mov     dx, 82E8h
        out     dx, ax
        OUTW    96E8h, 1023
        pop     ax
        dec     ax
        mov     dx, 0BEE8h
        out     dx, ax                  ; MIN_AXIS (index 0)
        OUTW    9AE8h, 40F3h
        call    idle
        ret

clr0:   mov     ax, 1
        xor     bx, bx
        jmp     fill0
clr40:  mov     ax, 12
        mov     bx, 40
        jmp     fill0

; PATTERN_L = AX, PATTERN_H = BX: 64x4 fill at (0,0) with FG 0Fh / BG 33h, record rows 0-3 x 0-63
pattern:
        push    bx
        push    ax
        mov     ax, 4
        xor     bx, bx
        call    fill0
        pop     ax
        mov     dx, 0BEE8h
        out     dx, ax
        pop     ax
        out     dx, ax
        OUTW    0A6E8h, 0Fh
        OUTW    0A2E8h, 33h
        OUTW    0BAE8h, 27h
        OUTW    0B6E8h, 07h
        OUTW    0BEE8h, 0A040h          ; PIX_CNTL: fixed pattern
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 63
        OUTW    0BEE8h, 3
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0BEE8h, 0A000h
        mov     word [row], 0
.r:     call    readrec64
        inc     word [row]
        cmp     word [row], 4
        jb      .r
        ret

; x-major line from (0, BX), 20 pixels, dy/dx = 1/4, ERR_TERM = AX, colour 0Fh
line:   push    ax
        call    idle
        OUTW    0A6E8h, 0Fh
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        mov     ax, bx
        mov     dx, 82E8h
        out     dx, ax
        OUTW    8AE8h, 2*5              ; AXSTP = 2*dy (dy 5)
        OUTW    8EE8h, 2*5 - 2*20       ; DIASTP = 2*dy - 2*dx
        pop     ax
        mov     dx, 92E8h               ; ERR_TERM
        out     dx, ax
        OUTW    96E8h, 20               ; MAJ_AXIS_PCNT
        OUTW    9AE8h, 20B9h            ; line, bit 3 as M8LINE, draw, +X +Y x-major, write
        call    idle
        ret

; record [row], x 0-255 (readrec) or x 0-63 (readrec64), at [recp]
readrec:
        mov     cx, 256
        jmp     rdgo
readrec64:
        mov     cx, 64
rdgo:   push    cx
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        mov     ax, [row]
        mov     dx, 82E8h
        out     dx, ax
        pop     cx
        push    cx
        mov     ax, cx
        dec     ax
        mov     dx, 96E8h
        out     dx, ax
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        pop     cx
        shr     cx, 1
        mov     di, [recp]
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        mov     [recp], di
        call    idle
        OUTW    0BAE8h, 27h
        ret

idle:   push    cx
        push    ax
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     ax
        pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
.dok:   pop     cx
        ret
