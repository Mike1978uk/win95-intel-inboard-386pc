; M8W128.COM - does 28800 register BDh bit 2 open a 128 KB VGA window at A0000-BFFFF?
;
; ATI.TXT names this bit for the 128 KB window; the earlier check (M8LIN) tried ACh bit 0 and
; GR06 map 00 only, and found B000 mirroring A000. ATI mode 62h, GR06 map 00, bank 0. In each arm:
; write 11h to A000:0, 22h to B000:0, read both. Distinct values mean two 64 KB stretches. Then
; read A000:0 with bank 1 selected: 22h means B000 is the next bank, a linear 128 KB.
;   arm 0  BDh as found: control. M8LIN predicts a mirror here (22h, 22h)
;   arm 1  BDh with bit 2 flipped
; BDh, B2h and GR06 are put back and text mode set before anything is written to disk.
; Writes M8W128.BIN: "M8WT", BDh as found, BDh after the flip read back, then for each arm
; A000:0, B000:0, A000:0 with bank 1. DOS boot.
;
;   nasm -f bin -o M8W128.COM M8W128.ASM

        org     100h

        jmp     start

oname   db      'M8W128.BIN', 0
m_ok    db      'M8W128: done', 13, 10, '$'
m_out   db      'M8W128: cannot write M8W128.BIN', 13, 10, '$'
obuf    db      'M8WT'
bdorg   db      0
bdflip  db      0
res     times 6 db 0
OUTLEN  equ     $ - obuf
b2org   db      0
gr6org  db      0

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

; DI -> 3 result bytes
arm:    mov     ah, [b2org]
        and     ah, 0E1h                ; write bank 0, read as found
        mov     al, 0B2h
        call    xwr
        mov     ax, 0A000h
        mov     es, ax
        mov     byte [es:0], 11h
        mov     ax, 0B000h
        mov     es, ax
        mov     byte [es:0], 22h
        mov     ax, 0A000h
        mov     es, ax
        mov     al, [es:0]
        mov     [di], al
        mov     ax, 0B000h
        mov     es, ax
        mov     al, [es:0]
        mov     [di + 1], al
        mov     ah, [b2org]
        and     ah, 0E1h
        or      ah, 02h                 ; bank 1 in B2h's write field
        mov     al, 0B2h
        call    xwr
        mov     ax, 0A000h
        mov     es, ax
        mov     al, [es:0]
        mov     [di + 2], al
        ret

start:
        cld
        mov     ax, 0062h
        int     10h
        mov     al, 0BDh
        call    xrd
        mov     [bdorg], al
        mov     al, 0B2h
        call    xrd
        mov     [b2org], al
        mov     dx, 3CEh
        mov     al, 6
        out     dx, al
        inc     dx
        in      al, dx
        mov     [gr6org], al
        and     al, 0F3h                ; map 00: A0000-BFFFF
        out     dx, al

        mov     di, res
        call    arm

        mov     ah, [bdorg]
        xor     ah, 04h
        mov     al, 0BDh
        call    xwr
        mov     al, 0BDh
        call    xrd
        mov     [bdflip], al
        mov     di, res + 3
        call    arm

        mov     ah, [bdorg]
        mov     al, 0BDh
        call    xwr
        mov     ah, [b2org]
        mov     al, 0B2h
        call    xwr
        mov     dx, 3CEh
        mov     al, 6
        mov     ah, [gr6org]
        out     dx, ax
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
