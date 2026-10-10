; M8CMPC.COM - DEST_CMP_FN's colour codes 08h-38h on an 8514/A CMD fill, Mach8 8 bpp.
;
; M8CMP measured codes 40h-78h (the Graphics Ultra nibble test) and found the card applies them to a
; CMD fill, which the model does not. This measures the colour compares (guide 9-50: 1 true, 2 >=,
; 3 <, 4 !=, 5 =, 6 <=, 7 >; TRUE keeps the pixel) on the same fill, so the model fix is measured,
; not guessed. As M8CMP otherwise: per colour (COLOR_CMP 08h, 55h, 80h) and code 00h,08h,...,38h,
; ramp row 0 00-FFh, set the compare, fill with EEh, compare off, read back.
; Writes M8CMPC.BIN: "M8CC", then per case: colour, code, 256 pixels. Engine only; DOS boot.

        org     100h

RES             equ     0800h
REC             equ     2 + 256
NCOL            equ     3
NCODE           equ     8

        jmp     start

oname   db      'M8CMPC.BIN', 0
m_ok    db      'M8CMPC: done, results in M8CMPC.BIN', 13, 10, '$'
m_out   db      'M8CMPC: cannot write M8CMPC.BIN', 13, 10, '$'
hdr     db      'M8CC'
cols    dw      08h, 55h, 80h
ci      dw      0
code    dw      0
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
.col:   mov     word [code], 0
.code:  call    ramp
        call    idle
        mov     bx, [ci]
        shl     bx, 1
        mov     ax, [cols + bx]
        mov     dx, 0B2E8h              ; COLOR_CMP
        out     dx, ax
        mov     dx, 0EEEEh              ; DEST_CMP_FN
        mov     ax, [code]
        out     dx, ax
        OUTW    0A6E8h, 0EEh            ; fill the row with EE, subject to the compare
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0               ; MIN_AXIS: one row
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0EEEEh, 0               ; compare off for the read
        mov     di, [recp]
        mov     bx, [ci]
        shl     bx, 1
        mov     al, [cols + bx]
        stosb
        mov     al, [code]
        stosb
        call    readrow
        add     word [recp], REC
        add     word [code], 8
        cmp     word [code], 40h
        jb      .code
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
        mov     cx, NCOL * NCODE * REC
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

; Row 0, x = 0..255, pixel x = x: 256 one-pixel fills.
ramp:   OUTW    0EEEEh, 0
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

; Read row 0 (256 pixels) into [di] with the 16-bit read proved on both machines.
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
