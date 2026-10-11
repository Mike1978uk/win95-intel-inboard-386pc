; M8BLEND.COM - what an in-engine blend costs, against the CPU doing the same over the bus.
;
; The Mach8's arithmetic mixes (guide 8-24: 17h average, 1Bh add clamped, 18h subtract clamped)
; blend source into destination inside the card. No ATI driver uses them; Windows GDI has only the
; Boolean mixes. The alternative is the CPU: read the destination back through PIX_TRANS, blend,
; write it again - every pixel crosses the 8-bit bus twice. Timed in BIOS ticks (55 ms):
;   1 NE engine blits 64x64 from off-screen (0,600) to (0,0), mix 07h copy
;   2 the same with mix 17h (D+S)/2
;   3 the same with mix 1Bh min(FFh, D+S)
;   4 NC CPU blends of the same 64x64: read 4096 pixels, average with the source, write them back
; Writes M8BLEND.BIN: "M8BL", NE, ticks 1, 2, 3, NC, ticks 4 (words). Engine only; DOS boot.

        org     100h

NE              equ     5000
NC              equ     40
SRCBUF          equ     4000h           ; 4096 source pixels for the CPU blend
DSTBUF          equ     5000h           ; 4096 destination pixels

        jmp     start

oname   db      'M8BLEND.BIN', 0
m_ok    db      'M8BLEND: done, results in M8BLEND.BIN', 13, 10, '$'
m_out   db      'M8BLEND: cannot write M8BLEND.BIN', 13, 10, '$'
hdr     db      'M8BL'
res     dw      NE, 0, 0, 0, NC, 0
mix     dw      0
cnt     dw      0
t0      dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        call    init
        call    fillsrc

        mov     word [mix], 67h         ; source = screen, copy
        call    eblits
        mov     [res+2], ax
        mov     word [mix], 77h         ; average
        call    eblits
        mov     [res+4], ax
        mov     word [mix], 7Bh         ; add, clamped
        call    eblits
        mov     [res+6], ax

        mov     di, SRCBUF              ; CPU path: the source once, into RAM
        mov     bx, 600
        call    rdblk
        call    tsync
        mov     word [cnt], NC
.c:     mov     di, DSTBUF
        xor     bx, bx
        call    rdblk                   ; destination back over the bus
        mov     si, SRCBUF
        mov     di, DSTBUF
        mov     cx, 4096
.avg:   mov     al, [si]
        add     al, [di]
        rcr     al, 1                   ; (D+S)/2 with the carry
        mov     [di], al
        inc     si
        inc     di
        loop    .avg
        call    wrblk                   ; and out again
        dec     word [cnt]
        jnz     .c
        call    tread
        mov     [res+10], ax

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, 12
        mov     dx, res
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

; NE blits with FRGD_MIX [mix]; AX = ticks taken
eblits: call    idle
        call    tsync
        mov     word [cnt], NE
.b:     call    idle
        mov     dx, 0BAE8h
        mov     ax, [mix]
        out     dx, ax
        OUTW    86E8h, 0                ; source (0,600)
        OUTW    82E8h, 600
        OUTW    8EE8h, 0                ; dest (0,0)
        OUTW    8AE8h, 0
        OUTW    96E8h, 63
        OUTW    0BEE8h, 63
        OUTW    9AE8h, 0C0F3h
        dec     word [cnt]
        jnz     .b
        call    idle
        OUTW    0BAE8h, 27h
        call    tread
        ret

; wait for a tick edge, remember it
tsync:  push    es
        xor     ax, ax
        mov     es, ax
        mov     ax, [es:046Ch]
.s:     cmp     ax, [es:046Ch]
        je      .s
        mov     ax, [es:046Ch]
        mov     [t0], ax
        pop     es
        ret
; AX = ticks since tsync
tread:  push    es
        xor     ax, ax
        mov     es, ax
        mov     ax, [es:046Ch]
        sub     ax, [t0]
        pop     es
        ret

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
        ret

; source block at (0,600) 64x64 and destination (0,0): one colour each
fillsrc:
        call    idle
        OUTW    0A6E8h, 0C0h
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 600
        OUTW    96E8h, 63
        OUTW    0BEE8h, 63
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0A6E8h, 40h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    9AE8h, 40F3h
        call    idle
        ret

; read 64x64 at (0,BX) into [di] through PIX_TRANS
rdblk:  call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        mov     ax, bx
        mov     dx, 82E8h
        out     dx, ax
        OUTW    96E8h, 63
        OUTW    0BEE8h, 63
        call    idle
        OUTW    9AE8h, 53B0h
        mov     cx, 2048
        mov     dx, 0E2E8h
.r:     push    dx
        call    dready
        pop     dx
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
        ret

; write 64x64 from DSTBUF to (0,0) through PIX_TRANS
wrblk:  call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 63
        OUTW    0BEE8h, 63
        OUTW    9AE8h, 53B1h
        mov     si, DSTBUF
        mov     cx, 2048
        mov     dx, 0E2E8h
        rep     outsw
        call    idle
        OUTW    0BAE8h, 27h
        ret

idle:   push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     dx
        pop     cx
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
