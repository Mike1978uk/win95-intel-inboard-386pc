; FLUSHP2.COM - sustained ISA memory rates, and the cost of a long I/O burst.
;
; M lines: 8 KB to B800:1000 (text pages 1-2, not shown) by rep stosb / stosw /
; stosd, then 8 KB read back by rep lodsb / lodsw / lodsd. Two runs each.
; One PIT count = 0.838 us; us/byte = count * 0.838 / 8192.
;
; B lines: four passes, each 4 x (op, re-read a 4 KB working set):
;   plain (no op), one OUT, 1000 OUTs, 1000 OUTs with no re-read.
; Refill after a long burst = (1000 OUTs) - (1000 OUTs, no re-read) - plain.
; The burst is rep outsb to 21h of the 8259 mask's current value, so the mask
; does not change; 21h only, never a word out (it would write 22h, the BL3 index).
;
;   FLUSHP2 > F2.TXT
;
; nasm -f bin -o FLUSHP2.COM FLUSHP2.ASM

        cpu     386
        org     100h

BUF     equ     8000h                   ; 4 KB working set
OUTB    equ     0A000h                  ; 1000 bytes of the mask value
NOUT    equ     1000

start:
        cld
        mov     dx, hdr
        mov     ah, 9
        int     21h
        in      al, 21h
        mov     di, OUTB                ; fill the burst source with the mask
        mov     cx, NOUT
        rep     stosb
        mov     ax, 0B800h
        mov     es, ax

; MEM tag, instruction, count
%macro MEM 3
        mov     byte [tag], %1
        cli
        call    pit
        mov     bp, bx
        mov     di, 1000h
        mov     si, 1000h
        mov     cx, %3
        %2
        call    pit
        sub     bp, bx
        mov     bx, [rp]
        mov     [bx], bp
        add     word [rp], 2
        sti
%endmacro

        MEM     'W', {rep stosb}, 8192
        MEM     'W', {rep stosb}, 8192
        MEM     'W', {rep stosw}, 4096
        MEM     'W', {rep stosw}, 4096
        call    putline                 ; W: stosb x2, stosw x2
        MEM     'D', {rep stosd}, 2048
        MEM     'D', {rep stosd}, 2048
        MEM     'D', {rep es lodsd}, 2048
        MEM     'D', {rep es lodsd}, 2048
        call    putline                 ; D: stosd x2, lodsd x2
        MEM     'R', {rep es lodsb}, 8192
        MEM     'R', {rep es lodsb}, 8192
        MEM     'R', {rep es lodsw}, 4096
        MEM     'R', {rep es lodsw}, 4096
        call    putline                 ; R: lodsb x2, lodsw x2

; BURST op, reread(1/0)
%macro BURST 2
        call    pit
        mov     bp, bx
        mov     di, 4
%%top:
        %1
%if %2
        mov     si, BUF
        mov     cx, 1024
        rep     lodsd
%endif
        dec     di
        jnz     %%top
        call    pit
        sub     bp, bx
        mov     bx, [rp]
        mov     [bx], bp
        add     word [rp], 2
%endmacro

%macro OP_NONE 0
%endmacro
%macro OP_ONE 0
        in      al, 21h
        out     21h, al
%endmacro
%macro OP_LONG 0
        mov     si, OUTB
        mov     dx, 21h
        mov     cx, NOUT
        rep     outsb
%endmacro

        mov     byte [tag], 'B'
        cli
        BURST   OP_NONE, 1
        BURST   OP_ONE, 1
        BURST   OP_LONG, 1
        BURST   OP_LONG, 0
        sti
        call    putline
        mov     byte [tag], 'B'
        cli
        BURST   OP_NONE, 1
        BURST   OP_ONE, 1
        BURST   OP_LONG, 1
        BURST   OP_LONG, 0
        sti
        call    putline

        mov     ax, 4C00h
        int     21h

pit:
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     bl, al
        in      al, 40h
        mov     bh, al
        ret

putline:
        mov     dl, [tag]
        mov     ah, 2
        int     21h
        mov     si, res
        mov     cx, 4
.w:     mov     dl, ' '
        mov     ah, 2
        int     21h
        lodsw
        call    puthex
        loop    .w
        mov     dl, 13
        mov     ah, 2
        int     21h
        mov     dl, 10
        int     21h
        mov     word [rp], res
        ret

puthex:
        push    cx
        mov     cx, 4
.d:     rol     ax, 4
        push    ax
        and     al, 0Fh
        add     al, '0'
        cmp     al, '9'
        jbe     .p
        add     al, 7
.p:     mov     dl, al
        mov     ah, 2
        int     21h
        pop     ax
        loop    .d
        pop     cx
        ret

hdr     db      'FLUSHP2 1: PIT counts', 13, 10, '$'
tag     db      0
rp      dw      res
res     dw      0, 0, 0, 0
