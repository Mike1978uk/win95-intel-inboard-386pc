; M8BLRD.COM - what an 8514 BitBLT read (CMD C3F0h) returns through PIX_TRANS on the Mach8.
;
; ATI TEST.COM TS2 sub-test 15 (writes 534Fh, compare 5336h) draws an 8x4 rectangle of host colour data at
; (0,206h) and reads it back with a BitBLT read; the card returns 0000 1010 2020 3030. The guide (pp. 8-29,
; 8-34) says a BitBLT read works in NIBBLE mode, one byte per 4-pixel nibble, but not what the byte holds.
; TS2's pixels share their high nibble per row, so "AND of the nibble's pixels" and "one pixel AND F0h"
; both fit. Each case here repeats sub-test 15's tables with its own pixels:
;   case 0  TS2's own data, RD_MASK FFh, source x 0   (must give 0000 1010 2020 3030)
;   case 1  discriminating data, RD_MASK FFh, source x 0
;   case 2  discriminating data, RD_MASK 0Fh, source x 0
;   case 3  discriminating data, RD_MASK FFh, source x 2 (destination x 10)
; Each record: idle and data-ready timeout counts, GP_STAT, 8 PIX_TRANS words read after the BitBLT
; (TS2 reads 4), then rows 206h-209h x 0-15 read back with the rectangle read M8BYTE proved (32 words).
; Writes M8BLRD.BIN ("M8BL", count, 4 records of 84 bytes). Run from a DOS boot; engine only.

        org     100h

NTEST           equ     4
NREAD           equ     8
REC             equ     84
RES             equ     2000h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8BLRD.BIN', 0
m_ok    db      'M8BLRD: done, results in M8BLRD.BIN', 13, 10, '$'
m_out   db      'M8BLRD: cannot write M8BLRD.BIN', 13, 10, '$'
hdr     db      'M8BL'
hcount  dw      NTEST
tno     dw      0
recp    dw      0
to_idle db      0
to_data db      0

; Per case: data table, RD_MASK, source x.
cases   dw      dts2, 0FFh, 0
        dw      ddis, 0FFh, 0
        dw      ddis, 0Fh, 0
        dw      ddis, 0FFh, 2

; Pixels go high byte first in each word (TS2 sub-tests 2-11 fit, m8ts2_nugget_fit.py).
dts2    dw      0102h, 0304h, 0506h, 0708h, 1112h, 1314h, 1516h, 1718h
        dw      2122h, 2324h, 2526h, 2728h, 3132h, 3334h, 3536h, 3738h
; Per nibble, AND of pixels / pixel 0 AND F0h / pixel 3 AND F0h differ in most bytes.
ddis    dw      0F00Fh, 3CC3h, 8142h, 2418h     ; AND 00 00, px0 F0 80, px3 C0 10
        dw      0FF7Fh, 3F1Fh, 0FEFCh, 0F8F0h   ; AND 1F F0, px0 F0 F0, px3 10 F0
        dw      55AAh, 55AAh, 1113h, 171Fh      ; AND 00 11, px0 50 10, px3 A0 10
        dw      0103h, 070Fh, 80C0h, 0E0F0h     ; AND 01 80, px0 00 80, px3 00 F0

start:
        cld
        mov     dx, 22E8h               ; 8514 display on, as TEST.COM's 069Bh does
        mov     al, 53h
        out     dx, al
        call    delay
        OUTW    4AE8h, 7
        call    delay

        mov     word [recp], RES
        mov     word [tno], 0
.t:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        mov     di, [recp]              ; clear the record
        mov     cx, REC / 2
        xor     ax, ax
        rep     stosw
        call    setup
        call    clear
        mov     si, [tno]               ; SI -> this case's entry
        mov     ax, 6
        mul     si
        mov     si, ax
        add     si, cases
        call    draw
        call    idle
        call    blitrd
        call    idle
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        mov     dx, 9AE8h
        in      ax, dx
        mov     [di+2], ax
        call    readback
        add     word [recp], REC
        inc     word [tno]
        cmp     word [tno], NTEST
        jb      .t

        call    setup                   ; leave the engine idle and reset
        OUTW    4AE8h, 2                ; back to VGA
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      outerr
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, NTEST * REC
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     say
outerr: mov     dx, m_out
say:    mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Engine reset and a known state, as M8T2S1.
setup:  OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0EEEEh, 0
        OUTW    92EEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BAEEh, 7
        OUTW    0B6EEh, 7
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        ret

; Rows 206h-209h x 0-31 to colour 00, so the destination starts known.
clear:  OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        OUTW    82E8h, 206h
        OUTW    96E8h, 31
        OUTW    0BEE8h, 3
        OUTW    9AE8h, 40B1h
        call    idle
        ret

; Sub-test 15's write table with this case's data and source x.
draw:   OUTW    0CEEEh, 4211h
        mov     ax, [si+4]
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 206h
        OUTW    96E8h, 7
        OUTW    0BEE8h, 3
        OUTW    9AE8h, 43F1h
        push    si
        mov     si, [si]
        mov     cx, 16
        mov     dx, 0E2E8h
.w:     lodsw
        out     dx, ax
        loop    .w
        pop     si
        ret

; Sub-test 15's read, with this case's RD_MASK; destination is source x + 8.
blitrd: OUTW    0CEEEh, 2211h
        OUTW    0BAE8h, 67h
        mov     ax, [si+2]
        mov     dx, 0AEE8h
        out     dx, ax
        mov     ax, [si+4]
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 206h
        mov     ax, [si+4]
        add     ax, 8
        mov     dx, 8EE8h
        out     dx, ax
        OUTW    8AE8h, 206h
        OUTW    9AE8h, 0C3F0h
        mov     di, [recp]
        add     di, 4
        mov     cx, NREAD
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        ret

; Rows 206h-209h x 0-15, two pixels per word, with the rectangle read (M8BYTE, M8T2S1).
readback:
        call    setup
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data
        OUTW    86E8h, 0
        OUTW    82E8h, 206h
        OUTW    96E8h, 15
        OUTW    0BEE8h, 3
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        add     di, 4 + 2 * NREAD
        mov     cx, 32
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
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

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret
