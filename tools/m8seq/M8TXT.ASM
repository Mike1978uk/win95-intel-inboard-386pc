; M8TXT.COM - how the Mach8 engine places 1-bit CPU data in a line command, a shape Microsoft's
; Windows 3.1 8514.DRV uses, so the 86Box model can be checked against the real card.
;
; CMD 3319h is command 1, a line (LSB_FIRST, 16-bit, PCDATA, DRAW, write), here with PIX_CNTL
; A080h: each CPU bit picks the foreground or background mix. For comparison the same bits are
; run as command 2, a rectangle (5319h).
;
; MAJ_AXIS 7, MIN_AXIS 15, start (40h,48h), 8 words, 128 bits; bit g is bit (g mod 16) of word
; g/16. The line draws one row of 8 pixels; the rectangle uses MIN_AXIS as its height. Runs 0-6
; set bit g iff bit r of g is set, so the seven read-backs together give the bit index each pixel
; came from. Run 7 sends all ones (the area covered), run 8 all zeros. Foreground colour 0Fh,
; background 01h, the area cleared to 00h first, so 00 = not drawn. Each run reads back
; (20h,38h)-(5Fh,57h), 64x32, around the start point, since the direction bits are clear in
; both commands.
;
; Run from a DOS boot. Writes M8TXT.BIN: "M8T1", record count (word), then per record:
; command (word), run, idle timeouts, data timeouts, pad, 2048 bytes. The 8514 display is
; switched on while it runs and back to VGA at the end.

        org     100h

AREA_X          equ     20h
AREA_Y          equ     38h
AREA_W          equ     64
AREA_H          equ     32
CUR_X0          equ     40h
CUR_Y0          equ     48h
NRUN            equ     9
RECBUF          equ     1000h           ; one record, 8 + 2048 bytes

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8TXT.BIN', 0
m_ok    db      'M8TXT: done, results in M8TXT.BIN', 13, 10, '$'
m_out   db      'M8TXT: cannot write M8TXT.BIN', 13, 10, '$'
hdr     db      'M8T1'
hcount  dw      2 * NRUN
cmds    dw      3319h, 5319h
handle  dw      0
ccur    dw      0                       ; command under test
rcur    db      0                       ; run
to_idle db      0
to_data db      0

start:
        cld
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      outerr
        mov     [handle], ax
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        int     21h

        ; 8514 display on, as TEST.COM's 069Bh does
        mov     dx, 22E8h
        mov     al, 53h
        out     dx, al
        call    delay
        OUTW    4AE8h, 7
        call    delay

        mov     si, cmds
.cmd:   lodsw
        mov     [ccur], ax
        mov     byte [rcur], 0
.run:   push    si
        mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    setup
        call    clear
        call    draw
        call    idle
        call    setup                   ; a command still waiting for data is abandoned here
        call    area_read
        call    save
        pop     si
        inc     byte [rcur]
        cmp     byte [rcur], NRUN
        jb      .run
        cmp     si, cmds + 4
        jb      .cmd

        call    idle
        OUTW    4AE8h, 2                ; back to VGA
        mov     ah, 3Eh
        mov     bx, [handle]
        int     21h
        mov     dx, m_ok
        jmp     say
outerr: mov     dx, m_out
say:    mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Engine reset and the state every run starts from: 1 MB layout, pitch 1024, full scissors,
; masks open, compare off.
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
        ret

; Fill the read area with 00h.
clear:  OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        OUTW    0A6E8h, 0
        OUTW    86E8h, AREA_X
        OUTW    82E8h, AREA_Y
        OUTW    96E8h, AREA_W - 1
        OUTW    0BEE8h, AREA_H - 1
        OUTW    9AE8h, 40B3h
        call    idle
        ret

; The command under test, fed 8 words built for this run.
draw:   OUTW    0A6E8h, 0Fh             ; FRGD_COLOR
        OUTW    0A2E8h, 01h             ; BKGD_COLOR
        OUTW    0BAE8h, 27h             ; FRGD_MIX: foreground colour, replace
        OUTW    0B6E8h, 07h             ; BKGD_MIX: background colour, replace
        OUTW    0BEE8h, 0A080h          ; PIX_CNTL: CPU data selects the mix
        OUTW    86E8h, CUR_X0
        OUTW    82E8h, CUR_Y0
        OUTW    96E8h, 7
        OUTW    0BEE8h, 15
        call    idle
        mov     dx, 9AE8h
        mov     ax, [ccur]
        out     dx, ax
        xor     di, di                  ; g of bit 0 of this word
.w:     call    fifo
        call    mkword
        mov     dx, 0E2E8h
        out     dx, ax
        add     di, 16
        cmp     di, 128
        jb      .w
        ret

; AX = the word whose bit b is set iff run r selects g = DI + b.
mkword: xor     ax, ax
        mov     cl, [rcur]
        mov     bx, 15
.b:     shl     ax, 1
        cmp     cl, 7
        je      .one
        ja      .zero
        lea     dx, [di + bx]
        shr     dx, cl
        test    dx, 1
        jz      .zero
.one:   or      ax, 1
.zero:  dec     bx
        jns     .b
        ret

; Read the area straight back: 53B0h as M8GLYPH, two pixels per word.
area_read:
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data, source
        OUTW    86E8h, AREA_X
        OUTW    82E8h, AREA_Y
        OUTW    96E8h, AREA_W - 1
        OUTW    0BEE8h, AREA_H - 1
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, RECBUF + 8
        mov     cx, AREA_W * AREA_H / 2
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        ret
.rz:    xor     ax, ax
        rep     stosw
        ret

save:   mov     di, RECBUF
        mov     ax, [ccur]
        stosw
        mov     al, [rcur]
        stosb
        mov     al, [to_idle]
        stosb
        mov     al, [to_data]
        stosb
        xor     ax, ax
        stosw
        stosb
        mov     ah, 40h
        mov     bx, [handle]
        mov     cx, 8 + AREA_W * AREA_H
        mov     dx, RECBUF
        int     21h
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
        push    ax
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     ax
        pop     dx
        pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret
