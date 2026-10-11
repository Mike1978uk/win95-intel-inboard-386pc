; M8LIN.COM - can the 28800 show 128 KB of video memory without bank switching? (worklist B2, D5)
;
; On this 8-bit XT bus only the first 1 MB is reachable, so a "linear" frame buffer can be at most
; the VGA's own 128 KB window (graphics register 06h, memory map 00 = A0000-BFFFF). In mode 13h:
; write 11h,12h at bank 0 offset 0-1 and 22h,23h at bank 1 offset 0-1 (B2h, 64 KB window), then
; open the 128 KB window with bank 0 and read A000:0-1 and B000:0-1. If B000:0 reads 22h, the
; second 64 KB is reachable directly. Done for the four combinations of the two documented
; "linear" bits, ACh bit 0 and B6h bit 2 (ATI.TXT). Every register is restored.
; Writes M8LIN.BIN: "M8LN", the saved ACh, B6h, BEh, B2h, GR06 (5 bytes), then per case: ACh,
; B6h, GR06 as read back, and A000:0, A000:1, B000:0, B000:1 (7 bytes). DOS boot; ends in text mode.

        org     100h

RES             equ     0800h

        jmp     start

oname   db      'M8LIN.BIN', 0
m_ok    db      'M8LIN: done, results in M8LIN.BIN', 13, 10, '$'
m_out   db      'M8LIN: cannot write M8LIN.BIN', 13, 10, '$'
hdr     db      'M8LN'
sAC     db      0
sB6     db      0
sBE     db      0
sB2     db      0
sG6     db      0
ncase   dw      0

start:
        cld
        mov     ax, 0013h
        int     10h
        mov     di, RES
        mov     ah, 0ACh
        call    atird
        mov     [sAC], al
        stosb
        mov     ah, 0B6h
        call    atird
        mov     [sB6], al
        stosb
        mov     ah, 0BEh
        call    atird
        mov     [sBE], al
        stosb
        mov     ah, 0B2h
        call    atird
        mov     [sB2], al
        stosb
        mov     ah, 6
        call    grrd
        mov     [sG6], al
        stosb

        mov     word [ncase], 0
.case:  mov     al, [sAC]               ; case bit 0 -> ACh bit 0, case bit 1 -> B6h bit 2
        and     al, 0FEh
        test    byte [ncase], 1
        jz      .c1
        or      al, 1
.c1:    mov     ah, 0ACh
        call    atiwr
        mov     al, [sB6]
        and     al, 0FBh
        test    byte [ncase], 2
        jz      .c2
        or      al, 4
.c2:    mov     ah, 0B6h
        call    atiwr
        mov     al, [sBE]               ; single bank mode
        and     al, 0F7h
        mov     ah, 0BEh
        call    atiwr

        mov     al, [sG6]               ; 64 KB window at A000
        and     al, 0F3h
        or      al, 04h
        mov     ah, 6
        call    grwr
        push    es
        mov     ax, 0A000h
        mov     es, ax
        mov     al, 0                   ; bank 0
        call    bank
        mov     byte [es:0], 11h
        mov     byte [es:1], 12h
        mov     al, 2                   ; bank 1
        call    bank
        mov     byte [es:0], 22h
        mov     byte [es:1], 23h
        mov     al, 0
        call    bank
        pop     es

        mov     al, [sG6]               ; 128 KB window at A000-BFFFF
        and     al, 0F3h
        mov     ah, 6
        call    grwr

        mov     ah, 0ACh
        call    atird
        stosb
        mov     ah, 0B6h
        call    atird
        stosb
        mov     ah, 6
        call    grrd
        stosb
        push    ds
        mov     ax, 0A000h
        mov     ds, ax
        mov     al, [0]
        mov     ah, [1]
        mov     bx, 0B000h
        mov     ds, bx
        mov     bl, [0]
        mov     bh, [1]
        pop     ds
        stosw
        mov     ax, bx
        stosw

        inc     word [ncase]
        cmp     word [ncase], 4
        jb      .case

        mov     al, [sG6]               ; restore everything
        mov     ah, 6
        call    grwr
        mov     al, [sB2]
        mov     ah, 0B2h
        call    atiwr
        mov     al, [sBE]
        mov     ah, 0BEh
        call    atiwr
        mov     al, [sB6]
        mov     ah, 0B6h
        call    atiwr
        mov     al, [sAC]
        mov     ah, 0ACh
        call    atiwr
        push    di
        mov     ax, 0003h
        int     10h
        pop     di

        mov     [ncase], di
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
        mov     cx, [ncase]
        sub     cx, RES
        mov     dx, RES
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

; ATI extended register AH -> AL
atird:  mov     dx, 1CEh
        mov     al, ah
        out     dx, al
        inc     dx
        in      al, dx
        ret
; ATI extended register AH = AL
atiwr:  mov     dx, 1CEh
        xchg    al, ah
        out     dx, ax                  ; index in AL, data in AH
        ret
; single-bank select: B2h bits 1-4 = bank, AL = bank << 1
bank:   push    ax
        mov     ah, [sB2]
        and     ah, 0E1h
        or      al, ah
        mov     ah, 0B2h
        call    atiwr
        pop     ax
        ret
; VGA graphics register AH -> AL / AH = AL
grrd:   mov     dx, 3CEh
        mov     al, ah
        out     dx, al
        inc     dx
        in      al, dx
        ret
grwr:   mov     dx, 3CEh
        xchg    al, ah
        out     dx, ax
        ret
