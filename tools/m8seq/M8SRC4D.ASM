; M8SRC4D.COM - DP_CONFIG foreground source 4 on the Mach8, fourth pass. M8SRC4C showed a 4:1 reduction
; (each destination pixel = 11h x the set pixels among four source pixels) and 44h at destination pixels
; 31, 47 and 63 in every test. Those read source x F0h, 130h and 170h, beyond the 64 pixels the probes
; write. This pass reads that part of the source row as found, then clears it and repeats, and tests which
; source values count as set.
;
; Records 0-3: source row 46h, x B4h-1B3h, read back before anything is written.
; Then, per test, the blit of M8SRC4C (DP_CONFIG 8279h, ALU 07h, destination 00h, 64 pixels), read back:
;   4   row cleared to x 27Fh, no set pixel
;   5   cleared, FFh at 0
;   6   cleared, FFh at 64
;   7   cleared, FFh at 124 (x F0h)
;   8   cleared, FFh at 255
;   9   cleared, 01h at 0, 80h at 4, 0Fh at 8, F0h at 12, read mask FFh
;   10  as 9, read mask 01h
;
; Run from a DOS boot. Writes M8SRC4D.BIN: "M8S7", record count (word), then per record: idle timeouts,
; data timeouts, 2 pad, 64 bytes. The 8514 display is switched on while it runs and back to VGA at the
; end.

        org     100h

NTEST           equ     7
NPRE            equ     4
NREC            equ     NTEST + NPRE
DST_Y           equ     4Eh
SRC_Y           equ     46h
X0              equ     74h
X1              equ     0B4h            ; DEST_X_END / SRC_X_END, exclusive
XCLR            equ     280h            ; the source row is cleared up to here, exclusive
REC             equ     68
RES             equ     2000h
TSZ             equ     18

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8SRC4D.BIN', 0
m_ok    db      'M8SRC4D: done, results in M8SRC4D.BIN', 13, 10, '$'
m_out   db      'M8SRC4D: cannot write M8SRC4D.BIN', 13, 10, '$'
hdr     db      'M8S7'
hcount  dw      NREC
; read mask, then four (position, value) pairs; position FFFFh = unused
tests   dw      0FFh, -1, 0, -1, 0, -1, 0, -1, 0
        dw      0FFh, 0, 0FFh, -1, 0, -1, 0, -1, 0
        dw      0FFh, 64, 0FFh, -1, 0, -1, 0, -1, 0
        dw      0FFh, 124, 0FFh, -1, 0, -1, 0, -1, 0
        dw      0FFh, 255, 0FFh, -1, 0, -1, 0, -1, 0
        dw      0FFh, 0, 01h, 4, 80h, 8, 0Fh, 12, 0F0h
        dw      01h, 0, 01h, 4, 80h, 8, 0Fh, 12, 0F0h
prex    dw      0B4h, 0F4h, 134h, 174h
tcur    dw      0
recp    dw      0
rd_x     dw      0
rd_y     dw      0
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

        mov     word [recp], RES
        call    setup
        mov     si, prex
.pre:   mov     byte [to_idle], 0
        mov     byte [to_data], 0
        mov     ax, [si]
        mov     [rd_x], ax
        mov     word [rd_y], SRC_Y
        push    si
        call    readrow
        pop     si
        add     word [recp], REC
        add     si, 2
        cmp     si, prex + NPRE * 2
        jb      .pre

        mov     word [tcur], tests
.t:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    setup
        call    loadpatt
        call    clrsrc
        call    mksrc
        call    filldst
        mov     si, [tcur]
        mov     dx, 0AEE8h              ; RD_MASK for the blit
        mov     ax, [si]
        out     dx, ax
        call    blit
        call    idle
        OUTW    0AEE8h, 00FFh
        mov     word [rd_x], X0
        mov     word [rd_y], DST_Y
        call    readrow
        add     word [tcur], TSZ
        add     word [recp], REC
        cmp     word [tcur], tests + NTEST * TSZ
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
        mov     cx, NREC * REC
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
        mov     cx, 8
        mov     ax, 1111h
.p:     mov     dx, 8EEEh
        out     dx, ax
        out     dx, ax
        add     ax, 1111h
        loop    .p
        ret

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

; The source row cleared to 00h from X0 to XCLR.
clrsrc: OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        OUTW    0A6E8h, 0
        OUTW    86E8h, X0
        OUTW    82E8h, SRC_Y
        OUTW    96E8h, XCLR - X0 - 1
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        call    idle
        ret

; The test's set pixels, up to four (position, value) pairs.
mksrc:  mov     si, [tcur]
        add     si, 2
        mov     cx, 4
.s:     mov     bx, [si]
        cmp     bx, 0FFFFh
        je      .n
        add     bx, X0
        mov     al, [si+2]
        push    cx
        push    si
        call    pixel
        pop     si
        pop     cx
.n:     add     si, 4
        loop    .s
        call    idle
        ret

; Destination row filled with 00h.
filldst:
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, X0
        OUTW    82E8h, DST_Y
        OUTW    96E8h, X1 - X0 + 3
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        call    idle
        ret

; TS1's blit writes, in its order: source (X0, SRC_Y), destination (X0, DST_Y), 64 pixels.
blit:   OUTW    0BAEEh, 07h             ; ALU_FG_FN: replace
        OUTW    0CEEEh, 8279h           ; DP_CONFIG: TS1 command 145's
        OUTW    0C2EEh, 1
        OUTW    86E8h, X0
        OUTW    82E8h, DST_Y
        OUTW    0A6EEh, X0              ; DEST_X_START
        OUTW    0AAEEh, X1              ; DEST_X_END
        OUTW    8EE8h, X0               ; source X
        OUTW    8AE8h, SRC_Y            ; source Y
        OUTW    0B2EEh, X0
        OUTW    0BEEEh, X1
        OUTW    0AEEEh, DST_Y + 1       ; DEST_Y_END: starts the blit
        ret

; Read 64 pixels back from (rd_x, rd_y): 64x1, 53B0h, two pixels per word.
readrow:
        call    idle
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data, source
        mov     dx, 86E8h
        mov     ax, [rd_x]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [rd_y]
        out     dx, ax
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

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret
