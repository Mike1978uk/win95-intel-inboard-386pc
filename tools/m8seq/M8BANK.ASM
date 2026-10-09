; M8BANK.COM - how the Graphics Ultra's VGA half (an ATI 28800) maps register B2h to the 64 KB banks seen
; through A000h. Sources disagree: 86Box's model takes the write bank from bits 4-1 and, when BEh bit 3 is
; set, the read bank from bit 0 and bits 7-5; an AI answer gave read 1-0 and write 4-3.
;
; Sets ATI mode 62h (640x480, 256 colours) so A000h is a 64 KB banked window. Then, with BEh bit 3 clear
; and again with it set: for each of nine B2h values (0, then each single bit 1-7, then bit 0), clear
; A000:0000; write marker 10h+i through A000:0000 with value i; then read A000:0000 back with each value.
; Which values see which markers says which bits move the write bank and which the read bank.
; B2h and BEh are put back as found and text mode 3 is set again before exit.
; Run from a DOS boot. Writes M8BANK.BIN: "M8BK", the mode number after the set, B2h and BEh as found, then
; 9 bytes for BEh bit 3 clear and 9 for it set: byte r is what A000:0000 read with the r-th B2h value.
; Decode with m8bank_decode.py.
;
;   nasm -f bin -o M8BANK.COM M8BANK.ASM

        org     100h

        jmp     start

oname   db      'M8BANK.BIN', 0
m_ok    db      'M8BANK: done', 13, 10, '$'
m_out   db      'M8BANK: cannot write M8BANK.BIN', 13, 10, '$'
obuf    db      'M8BK'
mode    db      0
b2org   db      0
beorg   db      0
res     times 18 db 0
OUTLEN  equ     $ - obuf
vals    db      00h, 02h, 04h, 08h, 10h, 20h, 40h, 80h, 01h
NVAL    equ     $ - vals

; AL = index -> AL = value
xrd:    push    dx
        mov     dx, 1CEh
        out     dx, al
        inc     dx
        in      al, dx
        pop     dx
        ret

; AL = index, AH = value
xwr:    push    dx
        mov     dx, 1CEh
        out     dx, ax
        pop     dx
        ret

; AH = B2h value
setb2:  push    ax
        mov     al, 0B2h
        call    xwr
        pop     ax
        ret

; one pass with BEh = AH; results to [di]
pass:   mov     al, 0BEh
        call    xwr
        mov     ax, 0A000h
        mov     es, ax
        xor     bx, bx
.clr:   mov     ah, [vals + bx]
        call    setb2
        mov     byte [es:0], 0
        inc     bx
        cmp     bx, NVAL
        jb      .clr
        xor     bx, bx
.wr:    mov     ah, [vals + bx]
        call    setb2
        mov     al, bl
        add     al, 10h
        mov     [es:0], al
        inc     bx
        cmp     bx, NVAL
        jb      .wr
        xor     bx, bx
.rd:    mov     ah, [vals + bx]
        call    setb2
        mov     al, [es:0]
        mov     [di + bx], al
        inc     bx
        cmp     bx, NVAL
        jb      .rd
        ret

start:
        mov     ax, 0062h               ; ATI 640x480x256
        int     10h
        mov     ah, 0Fh
        int     10h
        mov     [mode], al
        mov     al, 0B2h
        call    xrd
        mov     [b2org], al
        mov     al, 0BEh
        call    xrd
        mov     [beorg], al

        mov     ah, [beorg]
        and     ah, 0F7h                ; BEh bit 3 clear
        mov     di, res
        call    pass
        mov     ah, [beorg]
        or      ah, 08h                 ; BEh bit 3 set
        mov     di, res + NVAL
        call    pass

        mov     ah, [beorg]
        mov     al, 0BEh
        call    xwr
        mov     ah, [b2org]
        call    setb2
        mov     ax, 0003h
        int     10h

        push    cs
        pop     ds
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .noout
        mov     bx, ax
        mov     ah, 40h
        mov     cx, OUTLEN
        mov     dx, obuf
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.noout: mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
