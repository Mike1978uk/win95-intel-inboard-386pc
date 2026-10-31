; OR one glyph into a string bitmap (screen.c, packed text). The glyph's
; rows are gwb bytes each, the bitmap's stride bytes; each glyph byte is
; shifted right by the glyph's bit offset and lands in two bitmap bytes.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Every glyph of every string runs this: it is the whole layout cost, and
; Watcom's loop reloads its far pointers from the stack each byte.
;
; M8_GLYPH_OR: DX:AX = first bitmap byte, CX:BX = glyph bits,
;              SI = stride | shift << 8, DI = rows | gwb << 8.
; The byte after a row's last may be OR-ed with 0: the bitmap must have
; one spare byte after its end. Keeps BP.

public  M8_GLYPH_OR

_TEXT   segment para public 'CODE'

        .386
        assume  cs:_TEXT

        align   16
M8_GLYPH_OR     proc    near
        push    bp
        mov     es, dx
        mov     fs, cx
        mov     dx, si
        mov     cl, dh                  ; shift
        xor     dh, dh                  ; stride
        mov     si, bx                  ; fs:si glyph
        mov     bx, di
        movzx   bp, bl                  ; rows
        movzx   bx, bh                  ; gwb
        mov     di, ax                  ; es:di bitmap
        sub     dx, bx                  ; stride less the bytes a row advances
row:
        mov     ch, bl
byte_:
        mov     ah, fs:[si]
        xor     al, al
        shr     ax, cl
        or      es:[di], ah
        or      es:[di + 1], al
        inc     si
        inc     di
        dec     ch
        jnz     byte_
        add     di, dx
        dec     bp
        jnz     row
        pop     bp
        ret
M8_GLYPH_OR     endp

_TEXT   ends

        end
