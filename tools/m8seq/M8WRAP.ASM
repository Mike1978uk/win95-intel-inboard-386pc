; M8WRAP.COM - at the 8514/A-compatible pitch, do rectangle fills and blits plot X modulo 1024? The ATI
; guide (pp. 8-21, 8-22) says every point is plotted at X mod 1024 and the blit source wraps at the pitch;
; the card has shown it for raster draws and lines (TEST.COM TS1/TS2). Wrapped, a draw past X 1023 lands
; at the left edge of the same row; addressed linearly, it lands on the next row.
;
; The pitch is left at the 8514/A-compatible one (4AE8h written, GE_PITCH never written). Each case: engine
; reset, scissors open to 1535, both screen edges (X 960-1023 and 0-63, Y 0-15) filled with 55h, then on
; rows 8 and 9 a blit source: 11h at X 1016-1023 and 22h at X 0-7 on row 8, 44h and 33h on row 9; then
; the case's operation; then the two edges are read back as M8CONF does (16-bit read 53B0h).
;   0  nothing (control)
;   1  8514/A rectangle fill, AAh, X 1016 width 16, Y 2-5
;   2  ATI extended blit (DEST_Y_END) filling CCh, X 1016-1031, Y 2-5
;   3  8514/A blit, source (1020,8) width 8 -> (24,8): the source crosses X 1023
;   4  8514/A blit, source (0,8) width 8 -> (1020,12): the destination crosses X 1023
; Run from a DOS boot. Writes M8WRAP.BIN: "M8WR", case count (word), then per case: idle timeouts, data
; timeouts, 2 zero bytes, 1024 bytes of X 960-1023, 1024 bytes of X 0-63 (Y 0-15, 64 per row).
; Decode with m8wrap_decode.py.
;
;   nasm -f bin -o M8WRAP.COM M8WRAP.ASM

        org     100h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

REC     equ     4 + 2048

        jmp     start

oname   db      'M8WRAP.BIN', 0
m_ok    db      'M8WRAP: done', 13, 10, '$'
m_out   db      'M8WRAP: cannot write M8WRAP.BIN', 13, 10, '$'
hdr     db      'M8WR'
        dw      NCASE
ohand   dw      0
areax   dw      0
areay   dw      0
recp    dw      0
to_idle db      0
to_data db      0

; 8514/A rectangle fill: colour, X, Y, width, height.
%macro  FILL 5
        dw      0A6E8h, %1, 2,  86E8h, %2, 2,  82E8h, %3, 2,  96E8h, %4 - 1, 2,  0BEE8h, %5 - 1, 2
        dw      9AE8h, 40F3h, 2
%endmacro

; Entries are port, value, width (1 or 2); a port of 0 ends the list.
setup   dw      0BEE8h, 1000h, 2,  0BEE8h, 2000h, 2,  0BEE8h, 35FFh, 2,  0BEE8h, 45FFh, 2
        dw      0DAEEh, 0000h, 2,  0DEEEh, 0000h, 2,  0E2EEh, 05FFh, 2,  0E6EEh, 05FFh, 2
        dw      0AAE8h, 00FFh, 2,  0AEE8h, 00FFh, 2,  0BEE8h, 0A000h, 2,  0BAE8h, 0027h, 2
        FILL    55h, 960, 0, 64, 16
        FILL    55h, 0, 0, 64, 16
        FILL    11h, 1016, 8, 8, 1
        FILL    22h, 0, 8, 8, 1
        FILL    44h, 1016, 9, 8, 1
        FILL    33h, 0, 9, 8, 1
        dw      0

; 8514/A blit: source CUR_X/CUR_Y, destination DESTX_DIASTP (8EE8h) / DESTY_AXSTP (8AE8h), the source
; selected by FRGD_MIX 67h (VRAM blit, replace).
%macro  BLIT 4
        dw      0BAE8h, 0067h, 2,  86E8h, %1, 2,  82E8h, %2, 2,  8EE8h, %3, 2,  8AE8h, %4, 2
        dw      96E8h, 7, 2,  0BEE8h, 0, 2,  9AE8h, 0C0F3h, 2
%endmacro

c0      dw      0
c1      FILL    0AAh, 1016, 2, 16, 4
        dw      0
c2      dw      0CEEEh, 2211h, 2,  0BAEEh, 0007h, 2,  0A6E8h, 00CCh, 2
        dw      86E8h, 1016, 2,  0A6EEh, 1016, 2,  0AAEEh, 1032, 2,  82E8h, 2, 2,  0AEEEh, 6, 2,  0
c3      BLIT    1020, 8, 24, 8
        dw      0
c4      BLIT    0, 8, 1020, 12
        dw      0
cases   dw      c0, c1, c2, c3, c4
NCASE   equ     ($ - cases) / 2

start:
        cld
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .noout
        mov     [ohand], ax
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        int     21h

        ; --- 8514 display on, as TEST.COM's 069Bh does; this also sets the compatible pitch ---
        mov     dx, 22E8h
        mov     ax, 53h
        out     dx, ax
        call    delay
        OUTW    4AE8h, 7
        call    delay

        xor     bp, bp
.case:  mov     byte [to_idle], 0
        mov     byte [to_data], 0
        OUTW    32EEh, 0                ; engine reset
        OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        mov     si, setup
        call    play
        call    idle
        mov     bx, bp
        shl     bx, 1
        mov     si, [cases + bx]
        call    play
        call    idle

        mov     word [areax], 960
        mov     word [areay], 0
        mov     word [recp], res + 4
        call    area_read
        mov     word [areax], 0
        mov     word [areay], 0
        mov     word [recp], res + 4 + 1024
        call    area_read
        mov     al, [to_idle]
        mov     [res], al
        mov     al, [to_data]
        mov     [res + 1], al
        mov     word [res + 2], 0
        mov     ah, 40h
        mov     bx, [ohand]
        mov     cx, REC
        mov     dx, res
        int     21h
        inc     bp
        cmp     bp, NCASE
        jb      .case

        call    idle
        OUTW    4AE8h, 2
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al
        mov     ax, 3
        int     10h
        mov     ah, 3Eh
        mov     bx, [ohand]
        int     21h
        mov     dx, m_ok
        jmp     .say
.noout: mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Play entries (port, value, width) from SI until a port of 0.
play:   call    fifo
        lodsw
        or      ax, ax
        jz      .done
        mov     dx, ax
        lodsw
        mov     bx, ax
        lodsw
        xchg    ax, bx
        cmp     bx, 1
        je      .byte
        out     dx, ax
        jmp     play
.byte:  out     dx, al
        jmp     play
.done:  ret

; Read [areax],[areay] + 64x16 into [recp]: full scissors, masks open, 53B0h, as M8CONF does.
area_read:
        call    idle
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h
        mov     dx, 86E8h
        mov     ax, [areax]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [areay]
        out     dx, ax
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        mov     cx, 512
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret
.rz:    xor     ax, ax
        rep     stosw
        ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
idle:   push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     dx
        pop     cx
        ret

dready: push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     dx
        pop     cx
        ret

fifo:   push    cx
        push    dx
        push    ax
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     ax
        pop     dx
        pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret

res:    times   REC db 0
