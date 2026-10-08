; M8BYTE3.COM - what loads the shared low-byte latch. M8BYTE2 found one latch for all ports, but a
; lone 86E9h = 01h after word writes gave CUR_X 13Ch: the latch held the fill colour 3Ch, not the
; low byte of the last word written. Each test fills row 60h (colour F), sets CUR_X 40h, len 4,
; colour 55h with words, then:
;   0  F 3Ch, 86E9h = 01h                       (13Ch again)
;   1  F 2Ah, 86E9h = 01h                       (12Ah: the fill colour loads it)
;   2  F 3Ch, word A6E8h = 0077h, 86E9h = 01h   (177h: a word's low byte loads it)
;   3  F 3Ch, word 9AE8h = 40B3h (draw), 86E9h = 01h, word 40B3h
; Writes M8BYTE3.BIN ("M8B3", 4 records, M8BYTE's format).

        org     100h

NTEST           equ     4
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

oname   db      'M8BYTE3.BIN', 0
m_ok    db      'M8BYTE3: done, results in M8BYTE3.BIN', 13, 10, '$'
m_out   db      'M8BYTE3: cannot write M8BYTE3.BIN', 13, 10, '$'
hdr     db      'M8B3'
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
        shl     bx, 1
        mov     cx, [wf + bx]
        mov     ax, 0
        mov     bx, 200h
        call    run
        OUTW    9AE8h, 40B3h
        mov     ax, 40h
        mov     bx, 4
        mov     cx, 55h
        call    run
        pop     bx
        shl     bx, 1
        jmp     [wjump + bx]
wf      dw      3Ch, 2Ah, 3Ch, 3Ch
wjump   dw      w0, w0, w2, w3
w2:     OUTW    0A6E8h, 0077h
w0:     OUTB    86E9h, 01h
        jmp     wcmd
w3:     OUTW    9AE8h, 40B3h
        call    idle
        OUTB    86E9h, 01h
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
