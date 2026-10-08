; M8T2S1.COM - ATI TEST.COM TS2 sub-test 1 (writes 5105h, compare 50ECh), split into write and read halves.
; After the usual setup: DP_CONFIG 4211h, LINEDRAW_OPT 4, CUR (0,200h), SCAN_TO_X 8, PIX_TRANS 5555h AAAAh AAAAh
; 5555h (a raster write of 8 pixels). Then:
;   record 0: row 200h x 0-511 read with the rectangle read (53B0h), proven against the card by M8BYTE
;   record 1: TS2's own read - DP_CONFIG 4210h, LINEDRAW_INDEX 0, LINEDRAW (0,200h)-(8,200h), 4 PIX_TRANS words
;             (stored at the start of the record)
; Writes M8T2S1.BIN ("M8B6", 2 records, M8BYTE's format).

        org     100h

NTEST           equ     2
ROW_W           equ     200h
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

oname   db      'M8T2S1.BIN', 0
m_ok    db      'M8T2S1: done, results in M8T2S1.BIN', 13, 10, '$'
m_out   db      'M8T2S1: cannot write M8T2S1.BIN', 13, 10, '$'
hdr     db      'M8B6'
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
        call    s1write
        call    idle
        cmp     bx, 0
        jne     .rd
        call    readrow
        jmp     .nx
.rd:    call    s1read
.nx:    call    idle
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

; TS2 sub-test 1's raster write, as TEST.COM's table 5105h.
s1write:
        OUTW    0CEEEh, 4211h
        OUTW    0A2EEh, 4
        OUTW    86E8h, 0
        OUTW    82E8h, 200h
        OUTW    0CAEEh, 8
        OUTW    0E2E8h, 5555h
        OUTW    0E2E8h, 0AAAAh
        OUTW    0E2E8h, 0AAAAh
        OUTW    0E2E8h, 5555h
        ret

; TS2 sub-test 1's read: line (0,200h)-(8,200h) in read mode, then 4 PIX_TRANS words as TEST.COM's 5074h.
s1read: OUTW    0CEEEh, 4210h
        OUTW    9AEEh, 0
        OUTW    0FEEEh, 0
        OUTW    0FEEEh, 200h
        OUTW    0FEEEh, 8
        OUTW    0FEEEh, 200h
        mov     di, [recp]
        add     di, 4
        mov     cx, 4
.l:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .l
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
