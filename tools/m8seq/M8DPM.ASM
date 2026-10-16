; M8DPM.COM - 1-bit CPU data through the Mach8 extended path, DP_CONFIG 3251h, as ATIM8.DRV's ExtTextOut
; feeds it (M8TIME's text candidates B and C). The 86Box model has not been checked against the card on
; this path; the driver's text routine would rely on it.
;
; Per case: clear a 32x16 area at x = 20h, row 600 + 16*case; set DP_CONFIG 3251h; CUR_X, DEST_X_START,
; DEST_X_END (exclusive), CUR_Y, DEST_Y_END (starts the operation); send the data words; reset the engine;
; read the area back with the 16-bit 8514/A read. Foreground 0Fh (replace), background 01h (replace),
; area cleared to 00h, so each pixel reads 0Fh, 01h, or 00h (not written).
;   cases 0-6  16x8 at x 20h, 8 words: bit b of word w is set iff bit r of (16w + b) is set, r = case.
;              The seven read-backs give the bit index each pixel came from.
;   case 7     16x8 at x 20h, all ones (the area covered)
;   case 8     12x8 at x 21h, 6 words of mixed data (row packing, unaligned X)
;   case 9     20x8 at x 23h, 10 words of mixed data
; Writes M8DPM.BIN: "M8D1" + 10 x 512 pixels. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8DPM.BIN', 0
m_ok    db      'M8DPM: done, results in M8DPM.BIN', 13, 10, '$'
m_out   db      'M8DPM: cannot write M8DPM.BIN', 13, 10, '$'
hdr     db      'M8D1'
buf     times 10 * 512 db 0
dat     times 16 dw 0
mixed   dw      8001h, 4002h, 2004h, 1008h, 0810h, 0420h, 0240h, 0180h
        dw      0F0F0h, 0CC33h, 0AA55h, 1234h, 5678h, 9ABCh, 0DEF0h, 0FFFFh
; per case: x, width, words
cases   dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      20h, 16, 8
        dw      21h, 12, 6
        dw      23h, 20, 10
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
        call    fill_data
        ; the expansion, ATIM8's register order
        mov     bx, [cn]
        mov     ax, bx
        shl     bx, 1
        add     bx, ax
        shl     bx, 1                   ; 6 bytes per case
        OUTW    0A6E8h, 0Fh             ; FRGD_COLOR
        OUTW    0A2E8h, 01h             ; BKGD_COLOR
        OUTW    0BAE8h, 27h             ; FRGD_MIX: FRGD_COLOR, replace
        OUTW    0B6E8h, 07h             ; BKGD_MIX: BKGD_COLOR, replace
        OUTW    0CEEEh, 3251h
        call    idle                    ; FIFO drained: up to 15 entries follow
        mov     ax, [cases + bx]
        mov     dx, 86E8h               ; CUR_X
        out     dx, ax
        mov     dx, 0A6EEh              ; DEST_X_START
        out     dx, ax
        add     ax, [cases + bx + 2]
        mov     dx, 0AAEEh              ; DEST_X_END
        out     dx, ax
        mov     dx, 82E8h               ; CUR_Y
        mov     ax, [ybase]
        add     ax, 2
        out     dx, ax
        mov     dx, 0AEEEh              ; DEST_Y_END, starts it
        add     ax, 8
        out     dx, ax
        mov     dx, 0E2E8h
        mov     si, dat
        mov     cx, [cases + bx + 4]
        rep     outsw
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
        cmp     word [cn], 10
        jb      .c

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 10 * 512
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

; dat[] for case cn: cases 0-6 the index bit r = cn; case 7 all ones; cases 8-9 the mixed table.
fill_data:
        mov     di, dat
        mov     ax, [cn]
        cmp     ax, 7
        jb      .idx
        je      .ones
        mov     si, mixed
        mov     cx, 16
        rep     movsw
        ret
.ones:  mov     ax, 0FFFFh
        mov     cx, 16
        rep     stosw
        ret
.idx:   mov     cl, al                  ; r
        xor     bx, bx                  ; w
.w:     xor     dx, dx                  ; word being built
        mov     si, 0                   ; b
.b:     mov     ax, bx
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        add     ax, si                  ; g = 16w + b
        shr     ax, cl
        test    ax, 1
        jz      .nb
        mov     ax, 1
        push    cx
        mov     cx, si
        shl     ax, cl
        pop     cx
        or      dx, ax
.nb:    inc     si
        cmp     si, 16
        jb      .b
        mov     [di], dx
        add     di, 2
        inc     bx
        cmp     bx, 8
        jb      .w
        ret

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
