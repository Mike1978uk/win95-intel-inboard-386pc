; M8BYTE2.COM - follow-up to M8BYTE: is the low byte held for a lone high-byte write kept per
; register or in one latch shared by all ports? Each test fills row 60h with 3Ch, sets CUR_X,
; length and colour 55h with words, sends the bytes listed, then (unless noted) a word 40B3h.
;   0  CUR_X 40h, len 4, 86E9h = 01h                  (M8BYTE test 2 again: 13Ch?)
;   1  CUR_X 40h, len 8, 86E9h = 01h                  (does the length move it?)
;   2  CUR_X 50h, len 4, 86E9h = 01h                  (does the old low byte matter?)
;   3  CUR_X 40h, len 4, A6E8h = 20h, 86E9h = 01h     (shared latch: 120h)
;   4  AAE8h = B3h, 9AE9h = 40h, no word command      (shared latch: draws)
;   5  BAEEh = 01h alone                              (ALU_FG_FN zero: run colour 00h if latched)
;   6  BAEEh = 01h, BAEFh = 00h                       (pair: colour 00h)
;   7  9AE8h = B3h, 9AE9h = 40h, no word command      (split command: draws)
; Writes M8BYTE2.BIN in M8BYTE's format ("M8B2", 8 write records).

        org     100h

NTEST           equ     8
ROW_W           equ     60h
REC             equ     516
RES             equ     2000h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

%macro  OUTB 2
        mov     dx, %1
        mov     al, %2
        out     dx, al
%endmacro

        jmp     start

oname   db      'M8BYTE2.BIN', 0
m_ok    db      'M8BYTE2: done, results in M8BYTE2.BIN', 13, 10, '$'
m_out   db      'M8BYTE2: cannot write M8BYTE2.BIN', 13, 10, '$'
hdr     db      'M8B2'
hcount  dw      NTEST
tno     dw      0
recp    dw      0
to_idle db      0
to_data db      0

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
        mov     bx, [tno]
        call    wtest
        call    idle
        call    readrow
        call    idle
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        mov     dx, 9AE8h
        in      ax, dx
        mov     [di+2], ax
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

; Engine reset and a known state: 1 MB layout, pitch 1024, full scissors, masks open,
; replace mixes.
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

; One horizontal run: CUR_X = AX, length BX pixels, colour CX, on row ROW_W.
run:    push    ax
        call    idle
        pop     ax
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, ROW_W
        mov     ax, bx
        dec     ax
        mov     dx, 96E8h
        out     dx, ax
        mov     ax, cx
        mov     dx, 0A6E8h
        out     dx, ax
        OUTW    0BEE8h, 0
        ret

wtest:  push    bx
        mov     ax, 0                   ; fill the row with 3Ch
        mov     bx, 200h
        mov     cx, 3Ch
        call    run
        OUTW    9AE8h, 40B3h
        pop     bx
        push    bx
        shl     bx, 1
        mov     ax, [wx + bx]
        mov     cx, [wl + bx]
        mov     bx, cx
        mov     cx, 55h
        call    run
        pop     bx
        shl     bx, 1
        jmp     [wjump + bx]
wx      dw      40h, 40h, 50h, 40h, 40h, 40h, 40h, 40h
wl      dw      4, 8, 4, 4, 4, 4, 4, 4
wjump   dw      w0, w0, w0, w3, w4, w5, w6, w7
w0:     OUTB    86E9h, 01h
        jmp     wcmd
w3:     OUTB    0A6E8h, 20h
        OUTB    86E9h, 01h
        jmp     wcmd
w4:     OUTB    0AAE8h, 0B3h
        OUTB    9AE9h, 40h
        ret
w5:     OUTB    0BAEEh, 01h
        jmp     wcmd
w6:     OUTB    0BAEEh, 01h
        OUTB    0BAEFh, 00h
        jmp     wcmd
w7:     OUTB    9AE8h, 0B3h
        OUTB    9AE9h, 40h
        ret
wcmd:   OUTW    9AE8h, 40B3h
        ret

; Read the write row back: 512 pixels, two per word, into the record at +4.
readrow:
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data
        OUTW    86E8h, 0
        OUTW    82E8h, ROW_W
        OUTW    96E8h, 511
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        add     di, 4
        mov     cx, 256
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
