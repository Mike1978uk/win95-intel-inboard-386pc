; M8PL2.COM - which pixels a Mach8 polygon-boundary line (CMD A0xxh) writes, in open space.
;
; The 8514/A family's polygon-boundary line writes one pixel per scan line (Mach32 guide) but
; not which one. TEST.COM's Test Sequence 1 draws eight of them clipped into a 7x7 box, too
; little to read the rule from. This draws 16, one at a time, from (48,16) on a cleared card
; with full scissors, and reads (16,0)-(79,31) straight back after each:
;   cases 0-7   Bresenham, octant bits 7-5 = 0..7, CMD A013h | octant << 5,
;               DESTY_AXSTP 4, DESTX_DIASTP 1FFEh, ERR_TERM 1FFEh (TEST.COM's values), length 8
;   cases 8-15  radial, direction 0..7, CMD A01Bh | direction << 5, length 4
; Colour = case + 1. Writes M8PL2.BIN: "M8L1", then per case 2 timeout bytes and 2048 pixels.
; Run from a DOS boot. Engine registers only; the display is not switched.

        org     100h

NCASE           equ     16
RES             equ     0800h
REC             equ     2 + 2048

        jmp     start

oname   db      'M8PL2.BIN', 0
m_ok    db      'M8PL2: done, results in M8PL2.BIN', 13, 10, '$'
m_out   db      'M8PL2: cannot write M8PL2.BIN', 13, 10, '$'
hdr     db      'M8L1'
case    dw      0
recp    dw      0
to_idle db      0
to_data db      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     word [case], 0
        mov     word [recp], RES
.c:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    init_clear
        call    idle
        OUTW    0BAE8h, 27h             ; FRGD_MIX: colour, source
        mov     dx, 0A6E8h
        mov     ax, [case]
        inc     ax
        out     dx, ax
        OUTW    86E8h, 48
        OUTW    82E8h, 16
        mov     ax, [case]
        cmp     ax, 8
        jae     .rad
        OUTW    96E8h, 8
        OUTW    8AE8h, 4                ; DESTY_AXSTP
        OUTW    8EE8h, 1FFEh            ; DESTX_DIASTP
        OUTW    92E8h, 1FFEh            ; ERR_TERM
        mov     ax, [case]
        mov     cl, 5
        shl     ax, cl
        or      ax, 0A013h
        jmp     .go
.rad:   OUTW    96E8h, 4
        mov     ax, [case]
        sub     ax, 8
        mov     cl, 5
        shl     ax, cl
        or      ax, 0A01Bh
.go:    mov     dx, 9AE8h
        out     dx, ax
        call    idle
        call    area_read
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        add     word [recp], REC
        inc     word [case]
        cmp     word [case], NCASE
        jb      .c

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
        mov     cx, NCASE * REC
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

init_clear:
        OUTW    42E8h, 9000h            ; reset, chip test normal
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h           ; MEM_CNTL, as M8GLYPH
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        call    scissors
        OUTW    0A6E8h, 0
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 3FFh
        OUTW    9AE8h, 40F3h            ; fill 1024x1024 with 0
        call    idle
        ret

scissors:
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        ret

; (16,0)-(79,31) with the 16-bit read M8GLYPH proved on both machines.
area_read:
        call    idle
        call    scissors
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 16
        OUTW    82E8h, 0
        OUTW    96E8h, 63
        OUTW    0BEE8h, 31
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        add     di, 2
        mov     cx, 1024
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

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     cx
        ret
