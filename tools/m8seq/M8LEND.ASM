; M8LEND.COM - which write makes the Graphics Ultra draw a LINEDRAW's end point. In M8CONF test 58 the
; card drew the end point of a line whose LINEDRAW_OPT (070Ch, bit 2 set) was last written several tests
; earlier, with engine resets and many 8514/A commands in between; lines drawn straight after writing
; 070Ch leave it out on both card and model.
;
; Each case: engine reset, the register state ATI's Windows 3.11 driver had before test 58, a 64x16
; area at (0,0) filled with 55h by an 8514/A rectangle fill, DP_CONFIG 2211h, ALU_FG_FN 7, FRGD_COLOR
; FFh, LINEDRAW_OPT 070Ch, then the case's own writes, then LINEDRAW (10,2)-(30,12), and the area is
; read back as M8CONF does (16-bit read 53B0h). Case 0 has no writes of its own and is the control.
; Run from a DOS boot. Writes M8LEND.BIN: "M8LE", case count (word), then per case: idle timeouts,
; data timeouts, 2 zero bytes, 1024 bytes. Decode with m8lend_decode.py.
;
;   nasm -f bin -o M8LEND.COM M8LEND.ASM

        org     100h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

REC     equ     4 + 1024

        jmp     start

oname   db      'M8LEND.BIN', 0
m_ok    db      'M8LEND: done', 13, 10, '$'
m_out   db      'M8LEND: cannot write M8LEND.BIN', 13, 10, '$'
hdr     db      'M8LE'
        dw      NCASE
ohand   dw      0
areax   dw      0
areay   dw      0
recp    dw      0
to_idle db      0
to_data db      0

; Entries are port, value, width (1 or 2); a port of 0 ends the list.
setup   dw      7AEEh, 000Ah, 2,  6AEEh, 00EAh, 2,  36EEh, 0005h, 1
        dw      0BEE8h, 0A000h, 2,  0BEE8h, 1400h, 2,  0BEE8h, 2400h, 2,  0BEE8h, 37FFh, 2
        dw      0BEE8h, 47FFh, 2,  0BAE8h, 0027h, 2,  0B6E8h, 0007h, 2,  0A2E8h, 0000h, 2
        dw      0BEE8h, 8000h, 2,  0BEE8h, 9000h, 2,  0AEE8h, 00FFh, 2,  0AAE8h, 00FFh, 2
        dw      0DAEEh, 0000h, 2,  0DEEEh, 0000h, 2,  0E2EEh, 03FFh, 2,  0E6EEh, 03FFh, 2
        dw      0
clear   dw      0A6E8h, 0055h, 2,  86E8h, 0, 2,  82E8h, 0, 2,  96E8h, 63, 2,  0BEE8h, 000Fh, 2
        dw      9AE8h, 40F3h, 2,  0
lineset dw      0CEEEh, 2211h, 2,  0BAEEh, 0007h, 2,  0A6E8h, 00FFh, 2,  0A2EEh, 070Ch, 2,  0
line    dw      9AEEh, 0, 2,  0FEEEh, 10, 2,  0FEEEh, 2, 2,  0FEEEh, 30, 2,  0FEEEh, 12, 2,  0

; An 8514/A command that draws away from the area: a 1x1 rectangle fill at (200,200).
%define CMD1    86E8h, 200, 2,  82E8h, 200, 2,  96E8h, 0, 2,  0BEE8h, 0, 2,  9AE8h, 40F3h, 2

c0      dw      0                                       ; control
c1      dw      32EEh, 0, 2,  0                         ; the engine reset M8CONF starts each test with
c2      dw      42E8h, 900Fh, 2,  42E8h, 400Fh, 2,  0   ; subsystem reset
c3      dw      CMD1, 0                                 ; one 8514/A command
c4      dw      0BAE8h, 0027h, 2,  0                    ; FRGD_MIX alone
c5      dw      CMD1, 0BAE8h, 0027h, 2,  0              ; command then FRGD_MIX, as test 58 had it
c6      dw      CMD1, 0CEEEh, 2211h, 2,  0              ; command then DP_CONFIG
c7      dw      CMD1, 0A2EEh, 070Ch, 2,  0              ; command then LINEDRAW_OPT again
cases   dw      c0, c1, c2, c3, c4, c5, c6, c7
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

        ; --- 8514 display on, as TEST.COM's 069Bh does ---
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
        mov     si, clear
        call    play
        call    idle
        mov     si, lineset
        call    play
        mov     bx, bp
        shl     bx, 1
        mov     si, [cases + bx]
        call    play
        call    idle
        mov     si, line
        call    play
        call    idle

        mov     word [areax], 0
        mov     word [areay], 0
        mov     word [recp], res + 4
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
