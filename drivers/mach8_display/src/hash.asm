; The caches' hash (screen.c, M8_HASH): per word v, a += v, b += a,
; c = rol(c, 3) ^ v; a trailing odd byte is one more step, zero-extended.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Every lookup of the text, string and bitmap caches runs this over cold RAM,
; and Watcom's loop keeps a temporary on the stack. Here the state stays in
; registers and the only memory read per step is the data word.
;
; M8_HASH_BYTES: DX:AX = M8_HASH far *, CX:BX = data far *, DI = byte count.
; M8_HASH_WORD:  DX:AX = M8_HASH far *, BX = the word.
; Both keep SI and BP.

public  M8_HASH_BYTES
public  M8_HASH_WORD

_TEXT   segment byte public 'CODE'

        .386
        assume  cs:_TEXT

M8_HASH_BYTES   proc    near
        push    si
        push    bp
        mov     fs, dx
        mov     si, ax
        mov     es, cx
        mov     ax, fs:[si]             ; a
        mov     bp, fs:[si + 2]         ; b
        mov     dx, fs:[si + 4]         ; c
        push    di
        mov     cx, di
        shr     cx, 1
        jz      odd
step:
        mov     di, es:[bx]
        add     bx, 2
        add     ax, di
        add     bp, ax
        rol     dx, 3
        xor     dx, di
        dec     cx
        jnz     step
odd:
        pop     di
        test    di, 1
        jz      store
        movzx   di, byte ptr es:[bx]
        add     ax, di
        add     bp, ax
        rol     dx, 3
        xor     dx, di
store:
        mov     fs:[si], ax
        mov     fs:[si + 2], bp
        mov     fs:[si + 4], dx
        pop     bp
        pop     si
        ret
M8_HASH_BYTES   endp

M8_HASH_WORD    proc    near
        push    si
        mov     fs, dx
        mov     si, ax
        mov     ax, fs:[si]
        add     ax, bx
        mov     fs:[si], ax
        add     fs:[si + 2], ax
        mov     dx, fs:[si + 4]
        rol     dx, 3
        xor     dx, bx
        mov     fs:[si + 4], dx
        pop     si
        ret
M8_HASH_WORD    endp

_TEXT   ends

        end
