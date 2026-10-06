; M8SRC.COM - which register writes switch a Mach8 SCAN_TO_X from DP_CONFIG's source to the
; 8514/A one (FRGD_MIX / FRGD_COLOR).
;
; TEST.COM's Test Sequence 1 draws three SCAN_TO_X pixels with DP_CONFIG = 6211h (source =
; video memory): the first and third take FRGD_COLOR, the second, with no register written
; before it, does not. This measures the rule. Rows 64-127 are filled with 5Ah (any video
; memory the source can point at), FRGD_COLOR = C3h, FRGD_MIX = 27h (colour, source). For each
; case: CUR_X/CUR_Y to (8*case, 200), DP_CONFIG = 6211h, the case's extra write(s), then
; SCAN_TO_X to 8*case + 1 (one pixel). Row 200 is read back: C3 = 8514/A source, 5A = video
; memory, 00 = nothing drawn. Writes M8SRC.BIN: "M8X1" + 128 pixels of row 200.
;   0 nothing                    1 FRGD_MIX 27h            2 FRGD_COLOR C3h
;   3 WRT_MASK FFh               4 PIX_CNTL A000h          5 BKGD_MIX 27h
;   6 RD_MASK FFh                7 COLOR_CMP 0             8 DEST_CMP_FN 0
;   9 FRGD_MIX, then a second SCAN_TO_X with nothing between (pixel 8*9+1)
;  10 CUR_X written after DP_CONFIG (to the same value)
;  11 SRC_X_START 0 (a Mach8 register)
; Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8SRC.BIN', 0
m_ok    db      'M8SRC: done, results in M8SRC.BIN', 13, 10, '$'
m_out   db      'M8SRC: cannot write M8SRC.BIN', 13, 10, '$'
hdr     db      'M8X1'
buf     times 128 db 0
case    dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
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
        ; clear rows 0-255 to 0, then rows 64-127 to 5Ah
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 0FFh
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0A6E8h, 5Ah
        OUTW    86E8h, 0
        OUTW    82E8h, 64
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 63
        OUTW    9AE8h, 40F3h
        call    idle
        ; source registers into the 5A area, for whatever DP_CONFIG's blit reads
        OUTW    8EE8h, 8                ; SRC_X (Mach8 view of DESTX_DIASTP)
        OUTW    8AE8h, 80               ; SRC_Y
        OUTW    0B6EEh, 8               ; SRC_X_START
        OUTW    0BAEEh, 300             ; SRC_X_END
        OUTW    0BEEEh, 80              ; SRC_Y_DIR
        OUTW    0A6E8h, 0C3h            ; FRGD_COLOR
        OUTW    0BAE8h, 27h             ; FRGD_MIX

        mov     word [case], 0
.c:     call    idle
        mov     ax, [case]
        mov     cl, 3
        shl     ax, cl
        mov     dx, 86E8h               ; CUR_X
        out     dx, ax
        OUTW    82E8h, 200              ; CUR_Y
        OUTW    0CEEEh, 6211h           ; DP_CONFIG: source = video memory
        mov     bx, [case]
        shl     bx, 1
        call    [extra + bx]
        call    scan1
        cmp     word [case], 9
        jne     .n
        call    idle
        call    scan1                   ; case 9: a second one, nothing between
.n:     inc     word [case]
        cmp     word [case], 12
        jb      .c

        ; read row 200, x 0-127
        call    idle
        OUTW    0CEEEh, 0
        OUTW    0BAE8h, 47h
        OUTW    0BEE8h, 0A000h
        OUTW    86E8h, 0
        OUTW    82E8h, 200
        OUTW    96E8h, 127
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, buf
        mov     cx, 64
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 128
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

; SCAN_TO_X to CUR_X + 1, where CUR_X is 8 * case (+1 for case 9's second).
scan1:  call    idle
        mov     ax, [case]
        mov     cl, 3
        shl     ax, cl
        inc     ax
        cmp     word [case], 9
        jne     .s
        cmp     byte [second], 0
        je      .first
        inc     ax
.first: mov     byte [second], 1
.s:     mov     dx, 0CAEEh
        out     dx, ax
        ret
second  db      0

extra   dw      x_none, x_fmix, x_fcol, x_wmask, x_pixcntl, x_bmix
        dw      x_rmask, x_ccmp, x_cmpfn, x_fmix, x_curx, x_srcxs
x_none:   ret
x_fmix:   OUTW    0BAE8h, 27h
          ret
x_fcol:   OUTW    0A6E8h, 0C3h
          ret
x_wmask:  OUTW    0AAE8h, 0FFh
          ret
x_pixcntl: OUTW   0BEE8h, 0A000h
          ret
x_bmix:   OUTW    0B6E8h, 27h
          ret
x_rmask:  OUTW    0AEE8h, 0FFh
          ret
x_ccmp:   OUTW    0B2E8h, 0
          ret
x_cmpfn:  OUTW    0EEEEh, 0
          ret
x_curx:   mov     ax, [case]
          mov     cl, 3
          shl     ax, cl
          mov     dx, 86E8h
          out     dx, ax
          ret
x_srcxs:  OUTW    0B6EEh, 8
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
