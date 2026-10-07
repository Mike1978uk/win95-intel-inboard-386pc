; M8SRC4B.COM - DP_CONFIG foreground source 4 on the Mach8, second pass. M8SRC4 found that source 4 with
; mono source "always 1" leaves the engine waiting (for host data, probably), and that TEST.COM's own
; setting (DP_CONFIG 8279h) gives a value that is not the source pixel. This pass separates the two.
;
; Same frame as M8SRC4: colour pattern loaded, source row 46h x 74h-B3h, destination row 4Eh, one 64-pixel
; blit with TS1's writes, destination read back.
;
; Tests (DP_CONFIG, ALU_FG_FN, source, destination fill, colour pattern, host data):
;   0   8219h  07h  gradient   3Ch  TS1's   bytes 00h-3Fh, MSB first, 32 words
;   1   8219h  07h  gradient   3Ch  TS1's   all 07h, 32 words
;   2   8279h  07h  TS1's bits 00h  TS1's   none        (TS1 command 145 with replace instead of add)
;   3   8279h  07h  all FFh    00h  TS1's   none        (every pixel foreground)
;   4   8279h  07h  all FFh    00h  ramp    none        (pattern bytes 00h-1Fh)
; "TS1's" colour pattern: PATT_DATA_INDEX 0, then 1111h 1111h 2222h ... 8888h 8888h.
;
; Run from a DOS boot. Writes M8SRC4B.BIN: "M8S5", test count (word), then per test: idle timeouts,
; data timeouts, 2 pad, 64 bytes. The 8514 display is switched on while it runs and back to VGA at the
; end.

        org     100h

NTEST           equ     5
DST_Y           equ     4Eh
SRC_Y           equ     46h
X0              equ     74h
X1              equ     0B4h            ; DEST_X_END / SRC_X_END, exclusive
REC             equ     68
RES             equ     2000h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8SRC4B.BIN', 0
m_ok    db      'M8SRC4B: done, results in M8SRC4B.BIN', 13, 10, '$'
m_out   db      'M8SRC4B: cannot write M8SRC4B.BIN', 13, 10, '$'
hdr     db      'M8S5'
hcount  dw      NTEST
; DP_CONFIG, ALU_FG_FN, source kind (0 gradient, 1 FF, 2 00, 3 TS1 bits), destination fill, x offset,
; colour pattern (0 TS1's, 1 ramp), host data (0 none, 1 ramp bytes, 2 all 07h)
tests   dw      8219h, 07h, 0, 3Ch, 0, 0, 1
        dw      8219h, 07h, 0, 3Ch, 0, 0, 2
        dw      8279h, 07h, 3, 00h, 0, 0, 0
        dw      8279h, 07h, 1, 00h, 0, 0, 0
        dw      8279h, 07h, 1, 00h, 0, 1, 0
tsbits  dw      0123h, 4567h, 89ABh, 0CDEFh
tcur    dw      0
recp    dw      0
dstx    dw      0
to_idle db      0
to_data db      0

start:
        cld
        ; 8514 display on, as TEST.COM's 069Bh does
        mov     dx, 22E8h
        mov     al, 53h
        out     dx, al
        call    delay
        OUTW    4AE8h, 7
        call    delay

        mov     word [tcur], tests
        mov     word [recp], RES
.t:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    setup
        call    loadpatt
        mov     si, [tcur]
        mov     ax, [si+8]
        add     ax, X0
        mov     [dstx], ax
        call    mksrc
        call    filldst
        call    blit
        call    host
        call    idle
        call    readrow
        add     word [tcur], 14
        add     word [recp], REC
        cmp     word [tcur], tests + NTEST * 14
        jb      .t

        call    idle
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

; Engine reset and the state every test starts from: 1 MB layout, pitch 1024, full scissors, masks
; open, destination and source compare off, background colour 0, mixes "replace".
setup:  OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        OUTW    0BEE8h, 5006h           ; MEM_CNTL: 1 MB layout, as M8GLYPH
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0EEEEh, 0               ; DEST_CMP_FN off
        OUTW    92EEh, 0                ; source compare off (M8SCMP)
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0A2E8h, 0               ; BKGD_COLOR
        OUTW    0BAEEh, 7               ; ALU_FG_FN: replace
        OUTW    0B6EEh, 7               ; ALU_BG_FN: replace
        ret

; The colour pattern exactly as TS1 command 142 loads it.
loadpatt:
        OUTW    82EEh, 0                ; PATT_DATA_INDEX
        mov     si, [tcur]
        cmp     word [si+10], 0
        jne     .ramp
        mov     cx, 8
        mov     ax, 1111h
.p:     mov     dx, 8EEEh
        out     dx, ax
        out     dx, ax
        add     ax, 1111h
        loop    .p
        ret
.ramp:  mov     cx, 16                  ; bytes 00h-1Fh, low byte first in each word
        mov     ax, 0100h
.q:     mov     dx, 8EEEh
        out     dx, ax
        add     ax, 0202h
        loop    .q
        ret

; Host data after the blit has started: none, bytes 00h-3Fh (MSB first, so the high byte is the
; lower-numbered one), or all 07h. 32 words, each after a FIFO-room check.
host:   mov     si, [tcur]
        mov     bx, [si+12]
        or      bx, bx
        jz      .done
        mov     cx, 32
        mov     ax, 0001h
        cmp     bx, 2
        jne     .w
        mov     ax, 0707h
.w:     push    ax
        call    fifo
        pop     ax
        mov     dx, 0E2E8h
        out     dx, ax
        cmp     bx, 2
        je      .n
        add     ax, 0202h
.n:     loop    .w
.done:  ret

; One pixel at (BX, SRC_Y) in colour AL.
pixel:  push    ax
        call    idle
        pop     ax
        mov     ah, 0
        mov     dx, 0A6E8h              ; FRGD_COLOR
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, bx
        out     dx, ax
        OUTW    82E8h, SRC_Y
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        ret

; The source row, one pixel at a time: gradient, FFh, 00h, or TS1's bits.
mksrc:  OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        mov     si, [tcur]
        mov     di, [si+4]              ; source kind
        xor     cx, cx                  ; pixel index
.s:     mov     bx, cx
        add     bx, X0
        cmp     di, 0
        jne     .k1
        mov     al, cl                  ; gradient
        jmp     .put
.k1:    cmp     di, 1
        jne     .k2
        mov     al, 0FFh
        jmp     .put
.k2:    cmp     di, 2
        jne     .k3
        xor     al, al
        jmp     .put
.k3:    push    cx                      ; TS1's bits: word cx/16, bit 15 - cx%16
        mov     si, cx
        shr     si, 4
        shl     si, 1
        mov     ax, [tsbits + si]
        and     cl, 0Fh
        mov     dx, 15
        sub     dl, cl
        mov     cl, dl
        shr     ax, cl
        pop     cx
        test    al, 1
        mov     al, 0
        jz      .put
        mov     al, 0FFh
.put:   call    pixel
        inc     cx
        cmp     cx, X1 - X0
        jb      .s
        call    idle
        ret

; Destination row filled with the test's colour.
filldst:
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        mov     si, [tcur]
        mov     ax, [si+6]
        mov     dx, 0A6E8h
        out     dx, ax
        OUTW    86E8h, X0
        OUTW    82E8h, DST_Y
        OUTW    96E8h, X1 - X0 + 3
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        call    idle
        ret

; TS1's blit writes, in its order: source (X0, SRC_Y), destination (dstx, DST_Y), 64 pixels.
blit:   mov     si, [tcur]
        mov     dx, 0BAEEh              ; ALU_FG_FN
        mov     ax, [si+2]
        out     dx, ax
        mov     dx, 0CEEEh              ; DP_CONFIG
        mov     ax, [si]
        out     dx, ax
        OUTW    0C2EEh, 1
        mov     dx, 86E8h
        mov     ax, [dstx]
        out     dx, ax
        OUTW    82E8h, DST_Y
        mov     dx, 0A6EEh              ; DEST_X_START
        mov     ax, [dstx]
        out     dx, ax
        mov     dx, 0AAEEh              ; DEST_X_END
        mov     ax, [dstx]
        add     ax, X1 - X0
        out     dx, ax
        OUTW    8EE8h, X0               ; source X
        OUTW    8AE8h, SRC_Y            ; source Y
        OUTW    0B2EEh, X0
        OUTW    0BEEEh, X1
        OUTW    0AEEEh, DST_Y + 1       ; DEST_Y_END: starts the blit
        ret

; Read the destination row back from X0: 64x1, 53B0h, two pixels per word.
readrow:
        call    idle
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data, source
        OUTW    86E8h, X0
        OUTW    82E8h, DST_Y
        OUTW    96E8h, 63
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        mov     word [di+2], 0
        add     di, 4
        mov     cx, 32
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret
.rz:    xor     ax, ax
        rep     stosw
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

fifo:   push    cx
        push    dx
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     dx
        pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret
