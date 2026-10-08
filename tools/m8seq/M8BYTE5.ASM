; M8BYTE5.COM - M8BYTE's pixel-transfer read tests under each FIFO_OPT (36EEh) setting. On the Mach8,
; 36EEh bit 1 HOST_8_ENA selects 8-bit (1) or 16-bit (0) host data I/O, bit 0 W_STATE_ENA (ATI guide
; p. 9-8). TEST.COM writes 0001h before its tests; the POST ROM's value is not readable. Row 61h, pixel x
; coloured x+1; 64 pixels read back (53B0h) three ways, each with 36EEh = 0, 1, 2, 3 written first:
;   tests 0-3   64 x IN E2E8h (low byte)     36EEh = 0, 1, 2, 3
;   tests 4-7   64 x IN E2E9h (high byte)    36EEh = 0, 1, 2, 3
;   tests 8-11  32 x IN word E2E8h           36EEh = 0, 1, 2, 3
; Leaves 36EEh = 0001h, as TEST.COM does. Cold-boot afterwards to get the ROM's value back.
; Writes M8BYTE5.BIN ("M8B5", 12 records, M8BYTE's format).

        org     100h

NTEST           equ     12
ROW_W           equ     60h
ROW_R           equ     61h
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

oname   db      'M8BYTE5.BIN', 0
m_ok    db      'M8BYTE5: done, results in M8BYTE5.BIN', 13, 10, '$'
m_out   db      'M8BYTE5: cannot write M8BYTE5.BIN', 13, 10, '$'
hdr     db      'M8B5'
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
        call    rtest
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
        OUTW    36EEh, 1                ; TEST.COM's value
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
        mov     ax, 40h                 ; the run under test, set with words
        mov     bx, 4
        mov     cx, 55h
        call    run
        pop     bx
        shl     bx, 1
        jmp     [wjump + bx]
wjump   dw      w0, w1, w2, w3, w4, w5, w6, w7
w0:     jmp     wcmd
w1:     OUTB    86E8h, 80h
        jmp     wcmd
w2:     OUTB    86E9h, 01h
        jmp     wcmd
w3:     OUTB    96E8h, 07h
        jmp     wcmd
w4:     OUTB    0A6E8h, 0AAh
        jmp     wcmd
w5:     OUTB    9AE8h, 0B3h
        ret
w6:     OUTB    9AE9h, 40h
        ret
w7:     OUTB    86E8h, 80h
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

; Read tests: row ROW_R pixel x = x + 1, then 64 pixels read three ways.
rtest:  mov     ax, bx                  ; 36EEh = test & 3, before anything is drawn or read
        and     ax, 3
        mov     dx, 36EEh
        out     dx, ax
        push    bx
        xor     si, si
.p:     mov     ax, si
        mov     bx, 1
        mov     cx, si
        inc     cx
        push    si
        call    run
        OUTW    82E8h, ROW_R
        OUTW    9AE8h, 40B3h
        pop     si
        inc     si
        cmp     si, 64
        jb      .p
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, ROW_R
        OUTW    96E8h, 63
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        add     di, 4
        pop     bx
        shr     bx, 2                   ; 0 low byte, 1 high byte, 2 words
        cmp     bx, 2
        je      .w
        mov     dx, 0E2E8h
        cmp     bx, 0
        je      .b
        inc     dx
.b:     mov     cx, 64
.bl:    push    dx
        call    dready
        pop     dx
        in      al, dx
        stosb
        loop    .bl
        ret
.w:     mov     cx, 32
.wl:    call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .wl
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
