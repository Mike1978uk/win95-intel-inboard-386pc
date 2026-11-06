; M8DACX.COM - do the VGA half's and the 8514 half's palette ports reach one table?
;
; A palette entry written through one port set and read through the other, both ways, each with
; values no other entry holds. A halves-have-separate-tables result reads back something else.
;   entry 77h: written by 3C8/3C9 (11h 22h 33h), read by 02EB/02ED
;   entry 78h: written by 3C8/3C9 (14h 25h 36h), read by 02EB/02ED: control against a stuck index
;   entry 79h: written by 02EC/02ED (01h 02h 03h), read by 3C7/3C9
;   entry 7Ah: written by 02EC/02ED (2Ah 15h 0Bh), read by 3C7/3C9
; Writes M8DACX.BIN: "M8DX", then 3 bytes per entry. Mode 13h while it runs; DOS boot.
;
;   nasm -f bin -o M8DACX.COM M8DACX.ASM

        org     100h

        jmp     start

oname   db      'M8DACX.BIN', 0
m_ok    db      'M8DACX: done', 13, 10, '$'
m_out   db      'M8DACX: cannot write M8DACX.BIN', 13, 10, '$'
obuf    db      'M8DX'
res     times 12 db 0
OUTLEN  equ     $ - obuf

; DX = data port, DI -> 3 bytes read
rd3:    mov     cx, 3
.r:     in      al, dx
        mov     [di], al
        inc     di
        loop    .r
        ret

; DX = data port, AL AH BL = the values
wr3:    out     dx, al
        mov     al, ah
        out     dx, al
        mov     al, bl
        out     dx, al
        ret

; AL = entry, DX = index port, then a pause for the DAC's own cycle
idx:    out     dx, al
        jmp     short $+2
        ret

start:
        mov     ax, 0013h
        int     10h
        mov     di, res

        mov     al, 77h
        mov     dx, 3C8h
        call    idx
        mov     dx, 3C9h
        mov     ax, 2211h
        mov     bl, 33h
        call    wr3
        mov     al, 78h
        mov     dx, 3C8h
        call    idx
        mov     dx, 3C9h
        mov     ax, 2514h
        mov     bl, 36h
        call    wr3

        mov     al, 77h
        mov     dx, 2EBh
        call    idx
        mov     dx, 2EDh
        call    rd3
        mov     al, 78h
        mov     dx, 2EBh
        call    idx
        mov     dx, 2EDh
        call    rd3

        mov     al, 79h
        mov     dx, 2ECh
        call    idx
        mov     dx, 2EDh
        mov     ax, 0201h
        mov     bl, 03h
        call    wr3
        mov     al, 7Ah
        mov     dx, 2ECh
        call    idx
        mov     dx, 2EDh
        mov     ax, 152Ah
        mov     bl, 0Bh
        call    wr3

        mov     al, 79h
        mov     dx, 3C7h
        call    idx
        mov     dx, 3C9h
        call    rd3
        mov     al, 7Ah
        mov     dx, 3C7h
        call    idx
        mov     dx, 3C9h
        call    rd3

        mov     ax, 0003h
        int     10h
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
