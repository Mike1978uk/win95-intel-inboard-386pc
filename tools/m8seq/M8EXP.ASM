; M8EXP.COM - the Mach8 extended 1-bit expansion path (DP_CONFIG 3251) on the real card, so the
; 86Box model can be checked on the driver's text fallback (docs/mach8_driver_design.md).
;
; Background (0,600)-(127,615) filled with 22h. Then, with DP_CONFIG = 3251h (foreground colour
; where the CPU bit is 1, data from PIX_TRANS), FRGD_COLOR 0Fh:
;   case 0  8x16 at (0,600),   transparent (BKGD_MIX = destination), words W[0..7]
;   case 1  64x16 at (16,600), transparent, words W[0..63]
;   case 2  13x5 at (88,600),  transparent, words W[0..4] (65 bits: packing across row ends)
;   case 3  13x5 at (104,600), opaque: BKGD_MIX = colour, BKGD_COLOR 55h, words W[0..4]
; Each operation: CUR_X, DEST_X_START, DEST_X_END, CUR_Y, DEST_Y_END, then the words.
; Then (0,600)-(127,615) is read back with the 8514/A 16-bit read. Writes M8EXP.BIN:
; "M8E1" + 2048 pixels. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8EXP.BIN', 0
m_ok    db      'M8EXP: done, results in M8EXP.BIN', 13, 10, '$'
m_out   db      'M8EXP: cannot write M8EXP.BIN', 13, 10, '$'
hdr     db      'M8E1'
buf     times 2048 db 0
words   dw      8001h, 0F00Fh, 1234h, 5AA5h, 00FFh, 0FF00h, 0C3C3h, 0A55Ah
        times 7 dw 8001h, 0F00Fh, 1234h, 5AA5h, 00FFh, 0FF00h, 0C3C3h, 0A55Ah

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

; expand x, y, width, height, nwords
%macro  EXPAND 5
        call    idle
        OUTW    86E8h, %1
        OUTW    0A6EEh, %1
        OUTW    0AAEEh, %1 + %3
        OUTW    82E8h, %2
        OUTW    0AEEEh, %2 + %4
        mov     dx, 0E2E8h
        mov     si, words
        mov     cx, %5
%%w:    lodsw
        out     dx, ax
        loop    %%w
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
        OUTW    0CEEEh, 0
        ; background
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 22h
        OUTW    86E8h, 0
        OUTW    82E8h, 600
        OUTW    96E8h, 127
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 40F3h
        call    idle

        ; the expansions
        OUTW    0A6E8h, 0Fh             ; FRGD_COLOR
        OUTW    0BAE8h, 27h             ; foreground mix: colour, source
        OUTW    0B6E8h, 03h             ; background mix: destination (transparent)
        OUTW    0CEEEh, 3251h           ; DP_CONFIG, after the mixes
        EXPAND  0, 600, 8, 16, 8
        EXPAND  16, 600, 64, 16, 64
        EXPAND  88, 600, 13, 5, 5
        call    idle
        OUTW    0A2E8h, 55h             ; BKGD_COLOR
        OUTW    0B6E8h, 27h             ; background mix: colour, source (opaque)
        OUTW    0CEEEh, 3251h           ; again after the mix write
        EXPAND  104, 600, 13, 5, 5
        call    idle

        ; read back (0,600)-(127,615)
        OUTW    0CEEEh, 0
        OUTW    0BAE8h, 47h
        OUTW    0B6E8h, 07h
        OUTW    0BEE8h, 0A000h
        OUTW    86E8h, 0
        OUTW    82E8h, 600
        OUTW    96E8h, 127
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, buf
        mov     cx, 1024
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
        mov     cx, 4 + 2048
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
