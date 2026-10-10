; M8MIX.COM - the Mach8's 32 FRGD_MIX functions, including the arithmetic 10h-1Fh, in 8 bpp.
;
; The guide (8-24, 9-46..47) lists 16 arithmetic mixes after the 16 Boolean ones: MIN, MAX, add and
; subtract, saturated or not, and each halved. If the Mach8 has them, the engine does 50% blends and
; saturating light and shadow on its own. The model implements all 16; 18h = 19h and 1Ch = 1Dh in it.
;
; Per case: fill row 0 with the values 00-FFh (one 1x1 fill per pixel, paint), then fill the row
; with FRGD_COLOR by an 8514/A CMD under the case's mix, read the row back. Cases: mix 00h-1Fh x
; FRGD_COLOR 40h and C0h. Writes M8MIX.BIN: "M8MX", then per case: mix, colour, 256 pixels.
; WRT_MASK FFh, compare off. Engine only; DOS boot.

        org     100h

RES             equ     0800h
REC             equ     2 + 256
NMIX            equ     32
NCOL            equ     2

        jmp     start

oname   db      'M8MIX.BIN', 0
m_ok    db      'M8MIX: done, results in M8MIX.BIN', 13, 10, '$'
m_out   db      'M8MIX: cannot write M8MIX.BIN', 13, 10, '$'
hdr     db      'M8MX'
cols    dw      40h, 0C0h
ci      dw      0
mix     dw      0
recp    dw      0
pix     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        call    init
        mov     word [recp], RES
        mov     word [ci], 0
.col:   mov     word [mix], 0
.mix:   call    ramp
        call    idle
        mov     bx, [ci]
        shl     bx, 1
        mov     ax, [cols + bx]
        mov     dx, 0A6E8h              ; FRGD_COLOR
        out     dx, ax
        mov     ax, [mix]
        or      ax, 20h                 ; source FRGD_COLOR, 5-bit mix
        mov     dx, 0BAE8h
        out     dx, ax
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0               ; MIN_AXIS: one row
        OUTW    9AE8h, 40F3h
        call    idle
        mov     di, [recp]
        mov     al, [mix]
        stosb
        mov     bx, [ci]
        shl     bx, 1
        mov     al, [cols + bx]
        stosb
        call    readrow
        add     word [recp], REC
        inc     word [mix]
        cmp     word [mix], NMIX
        jb      .mix
        inc     word [ci]
        cmp     word [ci], NCOL
        jb      .col

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
        mov     cx, NMIX * NCOL * REC
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

init:
        OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        ret

; Row 0, x = 0..255, pixel x = x: 256 one-pixel fills, compare off, all planes.
ramp:   OUTW    0EEEEh, 0
        OUTW    0AAE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        mov     word [pix], 0
.p:     call    idle
        mov     dx, 0A6E8h
        mov     ax, [pix]
        out     dx, ax
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 0
        OUTW    9AE8h, 40F3h
        inc     word [pix]
        cmp     word [pix], 256
        jb      .p
        call    idle
        ret

; Read row 0 (256 pixels) into [di] with the 16-bit read proved on both machines (M8CMP).
readrow:
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     cx, 128
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     cx
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
