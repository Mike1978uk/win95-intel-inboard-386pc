; M8SCMP.COM - the Mach8 source colour compare (92EEh function, EAEEh colour) on a VRAM-to-VRAM blit,
; so the 86Box model can be checked against the real card. TEST.COM Test Sequence 1 uses it in its
; commands 133-140; no Windows driver writes either port.
;
; Source: row 46h, x 74h-B3h, sixteen 4-pixel groups coloured 00h, 11h ... FFh. Each test fills the
; destination row 4Eh, x 74h-B3h, with 3Ch, sets the background colour, 92EEh and EAEEh, copies the
; source row onto it with TS1's own blit writes (DP_CONFIG 6011h), and reads the 64 pixels back.
;
; Tests (92EEh, EAEEh, BKGD_COLOR):
;   0-7   function 00h, 08h ... 38h, colour 55h, background 69h
;   8     20h, AAh, 69h
;   9     20h, 55h, 7Eh (does a failing pixel take the background colour?)
;   10    92EEh = 08h, then an engine reset (42E8h 900Fh/400Fh), then the blit without writing 92EEh
;   11    as 10, with 32EEh = 0 (TEST.COM's reset) in place of 42E8h
;
; Run from a DOS boot. Writes M8SCMP.BIN: "M8C2", test count (word), then per test: idle timeouts,
; data timeouts, 2 pad, 64 bytes. The 8514 display is switched on while it runs and back to VGA at
; the end.

        org     100h

NTEST           equ     12
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

oname   db      'M8SCMP.BIN', 0
m_ok    db      'M8SCMP: done, results in M8SCMP.BIN', 13, 10, '$'
m_out   db      'M8SCMP: cannot write M8SCMP.BIN', 13, 10, '$'
hdr     db      'M8C2'
hcount  dw      NTEST
; 92EEh, EAEEh, BKGD_COLOR, kind (0 normal, 1 reset 42E8h before the blit, 2 reset 32EEh)
tests   dw      00h, 55h, 69h, 0
        dw      08h, 55h, 69h, 0
        dw      10h, 55h, 69h, 0
        dw      18h, 55h, 69h, 0
        dw      20h, 55h, 69h, 0
        dw      28h, 55h, 69h, 0
        dw      30h, 55h, 69h, 0
        dw      38h, 55h, 69h, 0
        dw      20h, 0AAh, 69h, 0
        dw      20h, 55h, 7Eh, 0
        dw      08h, 55h, 69h, 1
        dw      08h, 55h, 69h, 2
tcur    dw      0
recp    dw      0
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
        OUTW    92EEh, 0                ; compare off while the rows are prepared
        call    setup
        call    mksrc
        call    filldst
        mov     si, [tcur]
        OUTW    0A2E8h, [si+4]          ; BKGD_COLOR
        mov     dx, 0EAEEh
        mov     ax, [si+2]
        out     dx, ax
        mov     dx, 92EEh
        mov     ax, [si]
        out     dx, ax
        mov     ax, [si+6]
        cmp     ax, 1
        jne     .k2
        call    setup                   ; engine reset through 42E8h
        mov     si, [tcur]
        OUTW    0A2E8h, [si+4]
        jmp     .go
.k2:    cmp     ax, 2
        jne     .go
        OUTW    32EEh, 0                ; TEST.COM's 05F0h engine reset
        call    idle
        mov     si, [tcur]
        OUTW    0A2E8h, [si+4]
.go:    call    blit
        call    idle
        OUTW    92EEh, 0
        call    readrow
        add     word [tcur], 8
        add     word [recp], REC
        cmp     word [tcur], tests + NTEST * 8
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

; Engine reset and the state every test starts from: 1 MB layout, pitch 1024, full scissors,
; masks open, destination compare off, foreground and background mixes "replace".
setup:  OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        OUTW    0BEE8h, 5006h           ; MEM_CNTL: 1 MB layout, as M8GLYPH
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0EEEEh, 0               ; DEST_CMP_FN off
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BAEEh, 7               ; ALU_FG_FN: replace
        OUTW    0B6EEh, 7               ; ALU_BG_FN: replace
        ret

; Source row: sixteen 4-pixel groups, colour 11h times the group number.
mksrc:  OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        mov     cx, 16
        xor     bx, bx
.g:     call    idle
        mov     dx, 0A6E8h              ; FRGD_COLOR
        mov     al, bl
        mov     ah, 11h
        mul     ah
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, bx
        shl     ax, 2
        add     ax, X0
        out     dx, ax
        OUTW    82E8h, SRC_Y
        OUTW    96E8h, 3
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        inc     bx
        loop    .g
        call    idle
        ret

; Destination row filled with 3Ch.
filldst:
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 3Ch
        OUTW    86E8h, X0
        OUTW    82E8h, DST_Y
        OUTW    96E8h, X1 - X0 - 1
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40B3h
        call    idle
        ret

; TEST.COM TS1's one-row blit, its writes in its order: source (74h,46h), destination (74h,4Eh).
blit:   OUTW    0CEEEh, 6011h           ; DP_CONFIG
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

; Read the destination row back: 64x1, 53B0h, two pixels per word.
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
