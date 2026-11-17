; XTINSW.COM - time a 512-word rep insw from the XT-CF data port, for #50
; (does the L1 change it?). Run once with the BL3 cache on and once off;
; XTINSWA.BAT does both.
;
; Each test times four passes, interrupts masked per test (~70 ms) so
; COMrade's serial ISR keeps up. Output: one line per test, PIT counts in
; hex (0.838 us each), four passes.
;   A  4 x rep insw 512 words from 300h            transfer alone
;   B  4 x (rep insw 512 words, then rep lodsd     transfer plus the
;          over the 1 KB just read)                 driver touching the data
;   C  4 x rep lodsd over the same 1 KB             the touch alone
;
; The data port is read with no command pending: an ATA data read without
; DRQ has no side effect, and the bus cycles cost the same.
;
; C:\msys64\mingw64\bin\nasm.exe -f bin -o XTINSW.COM XTINSW.ASM

        cpu     386
        org     100h

DATA    equ     300h
BUF     equ     8000h
BURSTS  equ     4                       ; 4 x ~3.9 ms stays inside one PIT wrap

start:
        cld
        push    ds
        pop     es
        mov     dx, hdr
        mov     ah, 9
        int     21h

%macro OP_INSW 0
        mov     di, BUF
        mov     dx, DATA
        mov     cx, 512
        rep     insw
%endmacro
%macro OP_TOUCH 0
        mov     si, BUF
        mov     cx, 256
        rep     lodsd
%endmacro
%macro OP_BOTH 0
        OP_INSW
        OP_TOUCH
%endmacro

; PASS op -> PIT delta stored at [rp]
%macro PASS 1
        call    pit
        mov     bp, bx
        mov     word [cnt], BURSTS
%%top:
        %1
        dec     word [cnt]
        jnz     %%top
        call    pit
        sub     bp, bx                  ; PIT counts down
        mov     bx, [rp]
        mov     [bx], bp
        add     word [rp], 2
%endmacro

%macro TEST 2
        mov     byte [tag], %1
        cli
        PASS    %2
        PASS    %2
        PASS    %2
        PASS    %2
        sti
        call    putline
%endmacro

        TEST    'A', OP_INSW
        TEST    'B', OP_BOTH
        TEST    'C', OP_TOUCH

        mov     ax, 4C00h
        int     21h

; Latch PIT channel 0, count -> BX
pit:
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     bl, al
        in      al, 40h
        mov     bh, al
        ret

; Print "<tag> w0 w1 w2 w3", then reset rp
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

; AX as four hex digits
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

hdr     db      'XTINSW 1: 4 passes of 4 bursts (PIT counts)', 13, 10, '$'
tag     db      0
cnt     dw      0
rp      dw      res
res     dw      0, 0, 0, 0
