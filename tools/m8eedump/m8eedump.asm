; M8EEDUMP.COM - read all 64 words of an ATI Mach8 (Graphics Ultra) EEPROM.
;
; Follows the option ROM's own read routine (113-11504-002, 3A4Eh): the EEPROM is bit-banged
; through ATI extended register B3h at 1CEh/1CFh (bit 0 DI, bit 1 SK, bit 2 CS, bit 3 enable),
; DO is B7h bit 3, and A6h bit 2 is held clear for the duration. Only the READ opcode is ever
; clocked out, so the EEPROM cannot be written. Words are as the ROM sees them, which is the
; layout of 86Box's mach8.nvr.
;
; Writes M8EE.BIN (128 bytes, little-endian words) in the current directory.
;
;   nasm -f bin -o M8EEDUMP.COM m8eedump.asm

        org     100h

        mov     di, buf
        xor     bl, bl                  ; word number
next:   call    readword
        stosw
        inc     bl
        cmp     bl, 64
        jb      next

        mov     ah, 3Ch                 ; create file
        xor     cx, cx
        mov     dx, fname
        int     21h
        jc      fail
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 128
        mov     dx, buf
        int     21h
        jc      fail
        mov     ah, 3Eh
        int     21h
        mov     ax, 4C00h
        int     21h
fail:   mov     ax, 4C01h
        int     21h

; in: BL = word number. out: AX = word. Mirrors ROM 3A4Eh step for step.
readword:
        push    bx
        push    cx
        push    dx
        mov     dx, 1CEh
        mov     al, 0A6h
        pushf
        cli
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        popf
        push    ax                      ; saved A6h
        mov     ah, al
        and     ah, 0FBh
        mov     al, 0A6h
        out     dx, ax
        cli
        mov     al, 0B3h
        out     dx, al
        inc     dx
        in      al, dx
        mov     ah, al
        dec     dx
        mov     al, 0B3h
        sti
        call    clock
        and     ah, 0FEh                ; DI = 0
        out     dx, ax
        or      ah, 4                   ; CS
        out     dx, ax
        or      ah, 8                   ; enable
        out     dx, ax
        call    clock
        or      ah, 1                   ; start bit
        out     dx, ax
        call    clock
        or      ah, 1                   ; opcode 1
        out     dx, ax
        call    clock
        and     ah, 0FEh                ; opcode 0
        out     dx, ax
        call    clock
        mov     bh, 20h                 ; 6 address bits, MSB first
abit:   test    bh, bl
        jz      azero
        or      ah, 1
        out     dx, ax
        jmp     aclk
azero:  and     ah, 0FEh
        out     dx, ax
aclk:   call    clock
        shr     bh, 1
        jnz     abit
        and     ah, 0FEh
        out     dx, ax
        call    clock
        xor     bx, bx
        mov     cx, 16
dbit:   push    ax
        shl     bx, 1
        mov     al, 0B7h
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        test    al, 8
        jz      dzero
        or      bx, 1
dzero:  pop     ax
        call    clock
        loop    dbit
        and     ah, 0F7h
        out     dx, ax
        call    clock
        and     ah, 0FBh
        out     dx, ax
        pop     ax                      ; restore A6h
        mov     ah, al
        mov     al, 0A6h
        out     dx, ax
        call    delay
        mov     ax, bx
        pop     dx
        pop     cx
        pop     bx
        ret

clock:  or      ah, 2
        out     dx, ax
        call    delay
        and     ah, 0FDh
        out     dx, ax
        sti
delay:  push    cx
        mov     cx, 200                 ; the ROM uses 16; longer is harmless and spares SK timing doubts
dly:    loop    dly
        pop     cx
        ret

fname   db      'M8EE.BIN', 0
buf     times 128 db 0
