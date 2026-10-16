; M8SSV3.COM - decode EXT_SHORT_STROKE (C6EEh) on the card. M8SSV2 (10-16) drew one 45-degree diagonal per write where
; the guide (9-55) describes two IBM-format vectors, high byte first; the bed follows the guide.
; Here each write carries one vector, the other byte a null vector (00h: direction 0, move, length 0).
; Same set-up as M8SSV2: DP_CONFIG 2211h, FRGD_COLOR 0Fh, start at (30h, area + 8), area 32x16 at x 20h.
;   cases 0-7    high byte = direction (case) x 20h + draw 10h + length 4, low byte 00h
;   cases 8-15   low byte  = direction (case - 8) x 20h + draw 10h + length 4, high byte 00h
;   case 16      1F00h: direction 0, draw, length 15 (the longest)
;   case 17      0400h: direction 0, MOVE, length 4 (nothing should draw)
; Writes M8SSV3.BIN: "M8V3" + 18 x 512 pixels. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8SSV3.BIN', 0
m_ok    db      'M8SSV3: done, results in M8SSV3.BIN', 13, 10, '$'
m_out   db      'M8SSV3: cannot write M8SSV3.BIN', 13, 10, '$'
hdr     db      'M8V3'
buf     times 18 * 512 db 0
ssv     dw      1400h, 3400h, 5400h, 7400h, 9400h, 0B400h, 0D400h, 0F400h, 0014h, 0034h, 0054h, 0074h, 0094h, 00B4h, 00D4h, 00F4h, 1F00h, 0400h
cn      dw      0
ybase   dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     word [cn], 0
.c:     call    init
        mov     ax, [cn]
        mov     cl, 4
        shl     ax, cl
        add     ax, 600
        mov     [ybase], ax
        ; clear 32x16 at (20h, ybase) to 00h
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 20h
        mov     dx, 82E8h
        mov     ax, [ybase]
        out     dx, ax
        OUTW    96E8h, 31
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0A6E8h, 0Fh             ; FRGD_COLOR
        OUTW    0BAE8h, 27h             ; FRGD_MIX: FRGD_COLOR, replace
        OUTW    0CEEEh, 2211h           ; DP_CONFIG: foreground colour, draw
        call    idle
        OUTW    86E8h, 30h              ; CUR_X
        mov     dx, 82E8h               ; CUR_Y
        mov     ax, [ybase]
        add     ax, 8
        out     dx, ax
        mov     bx, [cn]
        shl     bx, 1
        mov     ax, [ssv + bx]
        mov     dx, 0C6EEh              ; EXT_SHORT_STROKE: two vectors
        out     dx, ax
        call    idle
        ; read the area back
        call    init
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 20h
        mov     dx, 82E8h
        mov     ax, [ybase]
        out     dx, ax
        OUTW    96E8h, 31
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     ax, [cn]
        mov     cl, 9
        shl     ax, cl
        add     ax, buf
        mov     di, ax
        mov     cx, 256
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        inc     word [cn]
        cmp     word [cn], 18
        jb      .c

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 18 * 512
        mov     dx, hdr
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

; Engine reset (clears any operation still waiting for data), pitch, scissors, masks, 8514/A data path.
init:   OUTW    42E8h, 9000h
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
        OUTW    0CEEEh, 0
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
