; M8SSV2.COM - EXT_SHORT_STROKE (C6EEh), the Mach8's packed short-stroke vectors: two IBM-format SSV bytes per
; 16-bit write, high byte first (guide 9-55). No Windows driver uses them (docs/mach8_levers.md); short strokes cost
; one bus write per two segments, a candidate for line art and glyph strokes.
;
; SSV byte: bits 7-5 direction (x 45 degrees), bit 4 draw (1) / move (0), bits 3-0 length.
; Per case: clear a 32x16 area at x = 20h, row 600 + 16*case; DP_CONFIG 2211h (foreground colour), FRGD_COLOR 0Fh;
; CUR_X 30h, CUR_Y area + 8; one write to C6EEh; reset the engine; read the area back (16-bit 8514/A read).
;   case 0  dir 0 draw 7, dir 2 draw 5        case 3  dir 5 draw 6, dir 7 draw 6
;   case 1  dir 4 draw 7, dir 6 draw 5        case 4  dir 0 move 4, dir 0 draw 8
;   case 2  dir 1 draw 6, dir 3 draw 6        case 5  dir 0 draw 0, dir 0 draw 15
; Writes M8SSV2.BIN: "M8V2" + 6 x 512 pixels. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8SSV2.BIN', 0
m_ok    db      'M8SSV2: done, results in M8SSV2.BIN', 13, 10, '$'
m_out   db      'M8SSV2: cannot write M8SSV2.BIN', 13, 10, '$'
hdr     db      'M8V2'
buf     times 6 * 512 db 0
ssv     dw      1755h, 97D5h, 3676h, 0B6F6h, 0418h, 101Fh
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
        cmp     word [cn], 6
        jb      .c

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 6 * 512
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
