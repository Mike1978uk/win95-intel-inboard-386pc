; M8GRAD.COM - can the polygon fill take its colours from somewhere other than one FRGD_COLOR?
; If so, a triangle can be a gradient or a texture for about the cost of a flat one.
;
; The guide's field table allows these DP_CONFIG values; none has been run on the card. The source
; holds its outline in plane 7 and a ramp in the other planes:
;   rows 500, 501, x 0-63: pixel = x on row 500, 40h + x on row 501; bit 7 also set at x 10 and x 50
; RD_MASK 80h makes plane 7 alone the outline. Each case fills rows `base, base + 1`, x 0-63 from
; those rows, after clearing them and resetting the engine:
;   case 0  base 0   2213h  FG FRGD_COLOR 77h: control, the path the driver uses. Must give 77h
;                           from x 10 up to the far edge and 0 outside
;   case 1  base 4   6213h  FG VRAM: the source pixel's own colour inside the outline?
;   case 2  base 8   A213h  FG colour pattern: bytes 80h + n, PATT_LENGTH 1Fh
;   case 3  base 12  E213h  FG source 7: the source byte indexes the colour pattern
;   case 4  base 16  6271h  FG VRAM, mono from VRAM, no polygon mode, background left alone: plane 7
;                           as a plain mask, for comparison
; Writes M8GRAD.BIN: "M8GR", 5 timeout bytes (1 = the case never went idle), then each case's two
; rows, 64 pixels each. Decode with m8vga_decode.py. Engine only; DOS boot.
;
;   nasm -f bin -o M8GRAD.COM M8GRAD.ASM

        org     100h

        jmp     start

oname   db      'M8GRAD.BIN', 0
m_ok    db      'M8GRAD: done', 13, 10, '$'
m_out   db      'M8GRAD: cannot write M8GRAD.BIN', 13, 10, '$'
obuf    db      'M8GR'
tout    times 5 db 0
res     times 5 * 2 * 64 db 0
OUTLEN  equ     $ - obuf
cases   dw      2213h, 0, 6213h, 4, 0A213h, 8, 0E213h, 12, 6271h, 16
cn      dw      0
base    dw      0
dpc     dw      0
pc      dw      0
px      dw      0
py      dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     di, res
        mov     word [cn], 0
.c:     call    init
        mov     bx, [cn]
        shl     bx, 1
        shl     bx, 1
        mov     ax, [cases + bx]
        mov     [dpc], ax
        mov     ax, [cases + bx + 2]
        mov     [base], ax
        call    source
        call    pattern

        mov     bx, [base]              ; clear the two destination rows
        call    clearrow
        inc     bx
        call    clearrow
        call    idle

        OUTW    0AEE8h, 0080h           ; RD_MASK: plane 7 is the outline
        OUTW    0AAE8h, 00FFh
        OUTW    0BAEEh, 0007h           ; ALU_FG_FN replace
        OUTW    0B6EEh, 0003h           ; ALU_BG_FN leave
        OUTW    0A6E8h, 0077h           ; FRGD_COLOR, for the control
        OUTW    0D2EEh, 001Fh           ; PATT_LENGTH
        OUTW    0D6EEh, 0               ; PATT_INDEX
        call    idle                    ; a full FIFO can lock the card: stay under 16 writes
        mov     dx, 0CEEEh
        mov     ax, [dpc]
        out     dx, ax
        OUTW    8EE8h, 0                ; SRC_X
        OUTW    8AE8h, 500              ; SRC_Y
        OUTW    0B2EEh, 0               ; SRC_X_START
        OUTW    0BEEEh, 64              ; SRC_X_END
        OUTW    0C2EEh, 1               ; SRC_Y_DIR
        OUTW    86E8h, 0                ; CUR_X
        OUTW    0A6EEh, 0               ; DEST_X_START
        OUTW    0AAEEh, 64              ; DEST_X_END
        mov     dx, 82E8h               ; CUR_Y
        mov     ax, [base]
        out     dx, ax
        mov     dx, 0AEEEh              ; DEST_Y_END: starts it
        mov     ax, [base]
        add     ax, 2
        out     dx, ax

        call    idle
        jnc     .idle
        mov     bx, [cn]
        mov     byte [tout + bx], 1
        call    init                    ; a stuck case must not poison the readback
.idle:
        OUTW    0AEE8h, 00FFh
        mov     bx, [base]
        call    readrow
        inc     bx
        call    readrow

        inc     word [cn]
        cmp     word [cn], 5
        jb      .c

        call    init
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, OUTLEN
        mov     dx, obuf
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
        call    idle
        OUTW    0DAEEh, 0
        OUTW    0DEEEh, 0
        OUTW    0E2EEh, 03FFh
        OUTW    0E6EEh, 03FFh
        ret

; rows 500 and 501, as the header says
source: push    di
        mov     word [py], 500
.r:     mov     word [px], 0
.x:     mov     ax, [px]
        cmp     word [py], 501
        jne     .n
        add     ax, 40h
        and     ax, 7Fh
.n:     cmp     word [px], 10
        je      .edge
        cmp     word [px], 50
        jne     .plot
.edge:  or      ax, 80h
.plot:  mov     [pc], ax
        call    plot
        inc     word [px]
        cmp     word [px], 64
        jb      .x
        inc     word [py]
        cmp     word [py], 502
        jb      .r
        call    idle
        pop     di
        ret

; colour pattern: byte n = 80h + n
pattern:
        OUTW    82EEh, 0
        mov     dx, 8EEEh
        mov     ax, 8180h
        mov     cx, 16
.p:     out     dx, ax
        add     ax, 0202h
        loop    .p
        ret

plot:   call    idle
        OUTW    0BAE8h, 27h
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        mov     dx, 0A6E8h
        mov     ax, [pc]
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, [px]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [py]
        out     dx, ax
        OUTW    9AE8h, 40F3h
        ret

; BX = row: x 0-63 to 00
clearrow:
        call    idle
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, bx
        out     dx, ax
        OUTW    96E8h, 63
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        ret

; BX = row: 64 pixels to [di]
readrow:
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, bx
        out     dx, ax
        OUTW    96E8h, 63
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     cx, 32
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret

; Carry set if the engine stayed busy for the whole bounded wait.
idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .ok
        loop    .i
        stc
        pop     cx
        ret
.ok:    clc
        pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .ok
        loop    .d
.ok:    pop     cx
        ret
