; A row of 8-bit pixels through a colour table, for m8_hw_write_xlat.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Four pixels a step, as ATIM8's DIB path does (seg12:0dec): one dword in,
; four table lookups, one dword out. No port access runs between rows, so the
; table and this loop stay in the L1 for the whole band.
;
; M8_XLAT: ES:DI = destination, DX:SI = source, AX:BX = 256-byte table
; (segment in AX), CX = pixels.

public  M8_XLAT

_TEXT   segment para public 'CODE'

        .386
        assume  cs:_TEXT

M8_XLAT proc    near
        push    ds
        mov     ds, ax
        mov     fs, dx
        mov     dx, cx
        shr     cx, 2
        jz      tail
quad:
        mov     eax, fs:[si]
        add     si, 4
        xlatb
        ror     eax, 8
        xlatb
        ror     eax, 8
        xlatb
        ror     eax, 8
        xlatb
        ror     eax, 8
        stosd
        dec     cx
        jnz     quad
tail:
        and     dx, 3
        jz      done
one:
        mov     al, fs:[si]
        inc     si
        xlatb
        stosb
        dec     dx
        jnz     one
done:
        pop     ds
        ret
M8_XLAT endp

_TEXT   ends

        end
